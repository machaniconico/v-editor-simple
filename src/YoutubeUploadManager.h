#pragma once

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QString>
#include <QTimer>
#include <QFile>
#include <QUuid>
#include <QPointer>

#include "YoutubeOAuth.h"
#include "YoutubeUploadClient.h"

// ---------------------------------------------------------------------------
// namespace youtube::manager — Sprint 17 US-YT-3
// YouTube アップロード Job キュー / 自動 retry / 進捗追跡 を司るマネージャ層。
//
// 責務:
//   - 複数の upload Job を FIFO キューで順次処理 (1 active job at a time)。
//   - oauth::AuthClient で token を refresh しつつ
//     upload::Client で resumable session を進める。
//   - 8MB chunk 単位で進捗報告と自動 retry (exponential backoff: 1s/2s/4s, max 3 回)。
//   - pause / resume / cancel API を提供し、UI 層から状態を制御可能にする。
//
// 設計方針:
//   - oauth::AuthClient は外部から注入 (caller が token のライフサイクルを所有)。
//   - upload::Client は内部所有。1 jobs 並列にしないため十分。
//   - 例外は投げず、必ず signal でエラー通知する。
// ---------------------------------------------------------------------------

namespace youtube {
namespace manager {

// Job の進行状態。UI 表示と内部 state machine の両方に使う。
enum class State {
    Queued,        // キュー待機中
    Authorizing,   // OAuth token を refresh 中
    Initiating,    // resumable session を初期化中
    Uploading,     // chunk を送信中
    Paused,        // ユーザによる一時停止
    Completed,     // 完了 (videoId 取得済み)
    Failed         // エラー or cancel
};

// 1 つの upload Job を表すレコード。
struct Job {
    QString id;                              // QUuid::createUuid().toString(WithoutBraces)
    QString filePath;                        // ローカル動画ファイル絶対 path
    youtube::upload::UploadMetadata metadata;
    QString sessionUri;                      // initiateSession 後に埋まる
    qint64 totalSize     = 0;                // QFileInfo::size()
    qint64 uploadedBytes = 0;                // 直近 chunkUploaded で更新
    State state          = State::Queued;
    QString errorMessage;                    // Failed 時の理由
    int retryAttempt     = 0;                // 現在の chunk に対する retry 回数 (0..3)
};

class Manager : public QObject {
    Q_OBJECT
public:
    explicit Manager(youtube::oauth::AuthClient* oauth, QObject* parent = nullptr);
    ~Manager() override;

    // Job をキューに追加。即座に jobAdded を emit し、idle なら processNext を始める。
    // 戻り値は新規 Job の id (QUuid 文字列, 中括弧なし)。
    // filePath が存在しない / 0 byte の場合は jobFailed を即時 emit。
    QString addJob(const QString& filePath, const youtube::upload::UploadMetadata& metadata);

    // 現在 active な job を一時停止。
    // active でない job は state=Paused にしてキューから外す。
    void pause(const QString& jobId);

    // Paused 状態の job を Queued に戻し、idle なら processNext を始める。
    void resume(const QString& jobId);

    // キュー or active から外し、state=Failed (errorMessage="cancelled") にする。
    void cancel(const QString& jobId);

    // 読み取り API (UI 用)。存在しなければ State::Failed を返す。
    State jobState(const QString& jobId) const;
    int   jobProgress(const QString& jobId) const;   // 0..100
    Job   jobSnapshot(const QString& jobId) const;   // コピー返却

    // 1 chunk のサイズ (8 MB)。テストから参照したいので公開。
    static constexpr qint64 kChunkSize = 8LL * 1024LL * 1024LL;
    static constexpr int    kMaxRetry  = 3;

signals:
    void jobAdded(const QString& jobId);
    void jobProgressChanged(const QString& jobId, int percent);
    void jobCompleted(const QString& jobId, const QString& videoId);
    void jobFailed(const QString& jobId, const QString& reason);
    void jobStateChanged(const QString& jobId, youtube::manager::State state);

private slots:
    // upload::Client からのシグナル受信
    void onSessionInitiated(const QString& sessionUri);
    void onSessionError(const QString& reason);
    void onChunkUploaded(qint64 newOffset);
    void onChunkError(const QString& reason);
    void onCompleted(const QString& videoId);

    // oauth::AuthClient からのシグナル受信 (Authorizing 中のみ反応)
    void onTokensReceived(const youtube::oauth::Token& token);
    void onAuthError(const QString& reason);

    // exponential backoff の delayed retry
    void retryCurrentChunk();

private:
    // キューから次の job を pop して active にし、Authorizing から開始する。
    // 既に m_currentJobId が動いていれば何もしない。
    void processNext();

    // current job の state を更新し jobStateChanged を emit する小ヘルパ。
    void setState(const QString& jobId, State newState);

    // current job の進捗 percent を再計算して jobProgressChanged を emit。
    void emitProgress(const QString& jobId);

    // current job について次の chunk を upload::Client に送る。
    // uploadedBytes >= totalSize なら何もしない (完了は upload::Client::completed が通知)。
    void pushNextChunk();

    // 失敗で current job を終了させる。
    void failCurrent(const QString& reason);

    // current job をクリアし、次の job を呼び出す。
    void finishCurrentAndAdvance();

    QPointer<youtube::oauth::AuthClient>  m_oauth;        // 外部所有 (non-owning)
    youtube::upload::Client*              m_uploadClient = nullptr; // 内部所有

    QHash<QString, Job> m_jobs;
    QQueue<QString>     m_queue;
    QString             m_currentJobId;     // 空 = idle
};

} // namespace manager
} // namespace youtube

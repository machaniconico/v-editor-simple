#include "ResourceGuide.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QGroupBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFont>
#include <QFrame>

// --- Static Resource Data ---

QVector<ResourceCategory> ResourceGuideDialog::allCategories()
{
    return {
        // Video / Footage
        { "動画素材 (Video Footage)", "🎬", {
            { "Pexels Videos",
              "https://www.pexels.com/videos/",
              "高品質な無料動画素材。商用利用OK、クレジット不要" },
            { "Pixabay Videos",
              "https://pixabay.com/videos/",
              "100万点以上の無料動画。商用利用OK" },
            { "Coverr",
              "https://coverr.co/",
              "ウェブ向け短尺動画。7日ごとに新素材追加" },
            { "Videvo",
              "https://www.videvo.net/",
              "無料動画クリップ・モーショングラフィックス" },
            { "Mixkit",
              "https://mixkit.co/free-stock-video/",
              "高品質な無料動画素材。商用利用OK" },
            { "Life of Vids",
              "https://lifeofvids.com/",
              "CC0ライセンスの自然・風景動画" }
        }},

        // Images / Photos
        { "画像・写真 (Images & Photos)", "📷", {
            { "Unsplash",
              "https://unsplash.com/",
              "高解像度写真。商用利用OK、クレジット不要" },
            { "Pexels Photos",
              "https://www.pexels.com/",
              "無料写真素材。商用利用OK" },
            { "Pixabay Images",
              "https://pixabay.com/",
              "写真・イラスト・ベクター画像" },
            { "StockSnap.io",
              "https://stocksnap.io/",
              "CC0ライセンスの高品質写真" },
            { "Burst (Shopify)",
              "https://burst.shopify.com/",
              "ビジネス向け無料写真" },
            { "ぱくたそ",
              "https://www.pakutaso.com/",
              "日本の無料写真素材。人物・風景が豊富" }
        }},

        // BGM / Music
        { "BGM・音楽 (Music)", "🎵", {
            { "DOVA-SYNDROME",
              "https://dova-s.jp/",
              "日本最大級のフリーBGMサイト。YouTube利用者多数" },
            { "甘茶の音楽工房",
              "https://amachamusic.chagasi.com/",
              "幅広いジャンルのフリーBGM。日本語" },
            { "魔王魂",
              "https://maou.audio/",
              "ゲーム・動画向けフリーBGM・効果音" },
            { "FreePD",
              "https://freepd.com/",
              "パブリックドメインの音楽素材" },
            { "Incompetech (Kevin MacLeod)",
              "https://incompetech.com/music/",
              "ジャンル豊富なCC音楽。クレジット表記で無料" },
            { "Mixkit Music",
              "https://mixkit.co/free-stock-music/",
              "高品質フリーBGM。商用利用OK" },
            { "YouTube Audio Library",
              "https://studio.youtube.com/channel/UC/music",
              "YouTube公式の無料音楽ライブラリ" }
        }},

        // Sound Effects
        { "効果音 (Sound Effects)", "🔊", {
            { "効果音ラボ",
              "https://soundeffect-lab.info/",
              "日本語の効果音サイト。カテゴリ分けが豊富" },
            { "Freesound",
              "https://freesound.org/",
              "ユーザー投稿型。CC/CC0ライセンス" },
            { "Zapsplat",
              "https://www.zapsplat.com/",
              "15万点以上の無料効果音" },
            { "SoundBible",
              "https://soundbible.com/",
              "CC/パブリックドメインの効果音" },
            { "OtoLogic",
              "https://otologic.jp/",
              "日本語。BGM・効果音・ジングル" },
            { "On-Jin ～音人～",
              "https://on-jin.com/",
              "日本語。システム音・生活音が充実" }
        }},

        // Fonts
        { "フォント (Fonts)", "🔤", {
            { "Google Fonts",
              "https://fonts.google.com/",
              "1500以上のオープンソースフォント" },
            { "FontFree",
              "https://fontfree.me/",
              "日本語フリーフォントまとめ" },
            { "FONTBEAR",
              "https://fontbear.net/",
              "商用利用可の日本語フリーフォント" },
            { "Font Meme",
              "https://fontmeme.com/",
              "映画・ブランド風フォント" },
            { "DaFont",
              "https://www.dafont.com/",
              "装飾フォントが豊富（ライセンス確認要）" }
        }},

        // Icons / Illustrations
        { "アイコン・イラスト (Icons & Illustrations)", "🎨", {
            { "unDraw",
              "https://undraw.co/illustrations",
              "カラー変更可能なSVGイラスト" },
            { "Flaticon",
              "https://www.flaticon.com/",
              "アイコン素材（無料はクレジット要）" },
            { "Icons8",
              "https://icons8.com/",
              "アイコン・写真・イラスト・音楽" },
            { "いらすとや",
              "https://www.irasutoya.com/",
              "日本で最も有名なフリーイラスト" },
            { "Loose Drawing",
              "https://loosedrawing.com/",
              "シンプルなフリーイラスト素材" },
            { "ICOOON MONO",
              "https://icooon-mono.com/",
              "モノクロアイコン素材。商用利用OK" }
        }},

        // Textures / Backgrounds
        { "テクスチャ・背景 (Textures & BG)", "🖼", {
            { "Subtle Patterns",
              "https://www.toptal.com/designers/subtlepatterns/",
              "繊細なタイルパターン背景" },
            { "Transparent Textures",
              "https://www.transparenttextures.com/",
              "透過テクスチャ素材" },
            { "Hero Patterns",
              "https://heropatterns.com/",
              "SVGベースの繰り返しパターン" },
            { "Poly Haven",
              "https://polyhaven.com/",
              "HDR環境マップ・テクスチャ。CC0" }
        }}
    };
}

// --- Dialog ---

ResourceGuideDialog::ResourceGuideDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Free Resource Guide — フリー素材ガイド");
    resize(700, 600);
    setupUI();
}

void ResourceGuideDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Header
    auto *headerLabel = new QLabel(
        "<h2>フリー素材ガイド</h2>"
        "<p style='color:#888;'>動画編集に使えるフリー素材サイト集。"
        "各サイトの利用規約を確認してからご利用ください。</p>");
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);

    // Scroll area
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *scrollWidget = new QWidget;
    auto *scrollLayout = new QVBoxLayout(scrollWidget);
    scrollLayout->setSpacing(12);

    auto categories = allCategories();
    for (const auto &cat : categories) {
        auto *group = new QGroupBox(QString("%1 %2").arg(cat.icon, cat.name));
        QFont groupFont = group->font();
        groupFont.setBold(true);
        groupFont.setPointSize(groupFont.pointSize() + 1);
        group->setFont(groupFont);

        auto *groupLayout = new QVBoxLayout(group);
        groupLayout->setSpacing(6);

        for (const auto &site : cat.sites) {
            auto *row = new QHBoxLayout;

            auto *linkBtn = new QPushButton(site.name);
            linkBtn->setCursor(Qt::PointingHandCursor);
            linkBtn->setFlat(true);
            linkBtn->setStyleSheet(
                "QPushButton { color: #4da6ff; text-align: left; font-weight: bold; "
                "text-decoration: underline; border: none; padding: 2px; }"
                "QPushButton:hover { color: #80c0ff; }");
            linkBtn->setFixedWidth(200);

            QString url = site.url;
            connect(linkBtn, &QPushButton::clicked, this, [this, url]() {
                openUrl(url);
            });

            auto *descLabel = new QLabel(site.description);
            descLabel->setWordWrap(true);
            descLabel->setStyleSheet("color: #aaa; padding: 2px;");

            row->addWidget(linkBtn);
            row->addWidget(descLabel, 1);
            groupLayout->addLayout(row);
        }

        scrollLayout->addWidget(group);
    }

    scrollLayout->addStretch();
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea, 1);

    // Close button
    auto *closeBtn = new QPushButton("閉じる (Close)");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    auto *bottomLayout = new QHBoxLayout;
    bottomLayout->addStretch();
    bottomLayout->addWidget(closeBtn);
    mainLayout->addLayout(bottomLayout);
}

void ResourceGuideDialog::openUrl(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}

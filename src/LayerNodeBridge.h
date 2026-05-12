#pragma once

#include "NodeGraph.h"
#include "VideoEffect.h"

/*
 * LayerNodeBridge — bidirectional conversion between ClipInfo effect stacks
 * and NodeGraph authoring view.
 *
 * Mapping table (VideoEffectType → node typeName):
 *   Blur       → "GaussianBlur"     (param1=radius → params["radius"])
 *   Invert     → "Invert"           (no params)
 *   Sharpen    → "Transform"        (unmapped — passthrough placeholder)
 *   Mosaic     → "Transform"        (unmapped)
 *   ChromaKey  → "Transform"        (unmapped)
 *   Vignette   → "Transform"        (unmapped)
 *   Sepia      → "Transform"        (unmapped)
 *   Grayscale  → "Transform"        (unmapped)
 *   Noise      → "Transform"        (unmapped)
 *
 * Only Blur and Invert have direct 1:1 built-in node equivalents.
 * Unmapped effects become a generic Transform node; toEffectStack will
 * reject such graphs (return false) because the round-trip is lossy.
 *
 * ColorCorrection (brightness/contrast/hue/saturation/etc.) lives on
 * ClipInfo separately from the VideoEffect stack and is NOT handled
 * by this bridge.
 */

namespace layerbridge {

// Builds a linear node chain: ImageInput(clipId) → one node per effect → Output.
// Nodes are laid out left-to-right on canvas.
NodeGraph fromEffectStack(const QString &clipId, const QVector<VideoEffect> &effects);

// Reverse-maps a simple linear chain graph back to a QVector<VideoEffect>.
// Returns true if the graph is a mappable linear chain; false otherwise
// (branches, multiple inputs, or unmapped node types).
// On false, outEffects is left untouched.
bool toEffectStack(const NodeGraph &graph, QVector<VideoEffect> &outEffects);

// Structural check: exactly one ImageInput, one Output, every intermediate
// node has exactly one incoming and one outgoing connection forming a
// single path from input to output.
bool isLinearChain(const NodeGraph &graph);

} // namespace layerbridge

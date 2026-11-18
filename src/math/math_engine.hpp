#pragma once

#include <QByteArrayView>
#include <QObject>
#include <QRgb>
#include <QString>

#include <functional>
#include <vector>

namespace peerchalk {

// Triangles sharing one colour.
struct MathRun {
    quint32 firstIndex{0};
    quint32 indexCount{0};
    quint32 rgba{0}; // 0xRRGGBBAA
};

// A laid-out formula. Lengths are in em; y grows down from the top of the formula, so the
// baseline is at y = height. When error is set the formula is empty.
struct MathFormula {
    std::vector<float> vertices; // x, y pairs
    std::vector<quint32> indices;
    std::vector<MathRun> runs;
    double width{0.0};
    double height{0.0};
    double depth{0.0};
    QString error;
};

struct MathRequest {
    QString tex;
    bool display{true};
    QRgb color{qRgba(0, 0, 0, 255)};
    float tolerance{0.002F}; // largest curve error, in em
};

using MathCallback = std::function<void(MathFormula)>;

// Lays out a formula away from the interface and calls done on receiver's thread. done is
// dropped if receiver is destroyed first. receiver must not be null.
void layoutMath(const MathRequest &request, QObject *receiver, MathCallback done);

[[nodiscard]] quint32 toMathRgba(QRgb color);

// Reads the math worker's result format (src/math/engine/src/wire.rs). Malformed data gives
// a formula with an error.
[[nodiscard]] MathFormula parseMathResult(QByteArrayView bytes);

} // namespace peerchalk

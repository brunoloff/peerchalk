#include "math/math_engine.hpp"
#include "math/math_smoothing.hpp"
#include "ui/math_item.hpp"

#include <QSignalSpy>
#include <QTest>
#include <QtEndian>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <stdexcept>

namespace peerchalk {
namespace {

constexpr int waitMs = 10000;

std::optional<MathFormula> layoutNow(const MathRequest &request) {
    QObject receiver;
    std::optional<MathFormula> result;
    layoutMath(request, &receiver, [&](MathFormula formula) { result = std::move(formula); });
    if (!QTest::qWaitFor([&] { return result.has_value(); }, waitMs))
        return std::nullopt;
    return result;
}

void appendWord(QByteArray &bytes, quint32 value) {
    const quint32 little = qToLittleEndian(value);
    bytes.append(reinterpret_cast<const char *>(&little), sizeof little);
}

// One triangle in one run, as the math worker sends it.
QByteArray workerResult(quint32 lastIndex = 2, quint32 runIndexCount = 3) {
    QByteArray bytes;
    for (const quint32 word : {3U, 3U, 1U, 0U})
        appendWord(bytes, word);
    bytes.append(QByteArray(24, '\0')); // width, height and depth of zero
    bytes.append(QByteArray(24, '\0')); // three vertices at the origin
    for (const quint32 word : {0U, 1U, lastIndex, 0U, runIndexCount, 0x112233ffU})
        appendWord(bytes, word);
    return bytes;
}

TEST(MathEngine, LaysOutAFormulaInTheRequestedColour) {
    const auto formula = layoutNow({QStringLiteral("\\frac{a}{b}"), true, qRgb(255, 0, 0)});
    ASSERT_TRUE(formula.has_value());
    EXPECT_TRUE(formula->error.isEmpty()) << formula->error.toStdString();
    EXPECT_GT(formula->width, 0.0);
    EXPECT_FALSE(formula->indices.empty());
    ASSERT_EQ(formula->runs.size(), 1U);
    EXPECT_EQ(formula->runs[0].rgba, 0xff0000ffU);
}

TEST(MathEngine, ReportsInvalidLatex) {
    const auto formula = layoutNow({QStringLiteral("\\frac{")});
    ASSERT_TRUE(formula.has_value());
    EXPECT_FALSE(formula->error.isEmpty());
    EXPECT_TRUE(formula->indices.empty());
}

TEST(MathEngine, DropsTheResultWhenTheReceiverIsGone) {
    bool called = false;
    auto receiver = std::make_unique<QObject>();
    layoutMath({QStringLiteral("x")}, receiver.get(), [&](MathFormula) { called = true; });
    receiver.reset();
    QTest::qWait(200);
    EXPECT_FALSE(called);
}

TEST(MathEngine, RejectsAMissingReceiver) {
    EXPECT_THROW(layoutMath({}, nullptr, [](MathFormula) {}), std::invalid_argument);
}

TEST(MathResult, ReadsWorkerData) {
    const MathFormula formula = parseMathResult(workerResult());
    EXPECT_TRUE(formula.error.isEmpty()) << formula.error.toStdString();
    EXPECT_EQ(formula.vertices.size(), 6U);
    EXPECT_EQ(formula.indices, (std::vector<quint32>{0, 1, 2}));
    ASSERT_EQ(formula.runs.size(), 1U);
    EXPECT_EQ(formula.runs[0].rgba, 0x112233ffU);
}

TEST(MathResult, RejectsMalformedData) {
    EXPECT_FALSE(parseMathResult(QByteArray(4, '\0')).error.isEmpty());
    EXPECT_FALSE(parseMathResult(workerResult().chopped(1)).error.isEmpty());
    EXPECT_FALSE(parseMathResult(workerResult(3)).error.isEmpty());    // index past the vertices
    EXPECT_FALSE(parseMathResult(workerResult(2, 4)).error.isEmpty()); // run past the indices
}

// A square of side 1 made of two triangles.
MathFormula unitSquare() {
    MathFormula formula;
    formula.vertices = {0, 0, 1, 0, 1, 1, 0, 1};
    formula.indices = {0, 1, 2, 0, 2, 3};
    formula.runs = {{0, 6, 0x000000ffU}};
    return formula;
}

TEST(MathSmoothing, FadesAcrossTheOutlineOnly) {
    const MathFormula square = unitSquare();
    const SmoothMesh mesh = smoothMathRun(square, square.runs[0], 0.04F);
    ASSERT_EQ(mesh.vertices.size(), 8U);
    EXPECT_EQ(mesh.indices.size(), 6U + 4U * 6U); // the shared diagonal gets no band
    EXPECT_FLOAT_EQ(mesh.vertices[0].x, 0.02F);
    EXPECT_FLOAT_EQ(mesh.vertices[0].y, 0.02F);
    EXPECT_FLOAT_EQ(mesh.vertices[0].alpha, 1.0F);
    EXPECT_FLOAT_EQ(mesh.vertices[4].x, -0.02F);
    EXPECT_FLOAT_EQ(mesh.vertices[4].y, -0.02F);
    EXPECT_FLOAT_EQ(mesh.vertices[4].alpha, 0.0F);
}

TEST(MathSmoothing, WeldsCopiesOfAPosition) {
    MathFormula formula;
    formula.vertices = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1}; // the diagonal's ends twice
    formula.indices = {0, 1, 2, 3, 4, 5};
    formula.runs = {{0, 6, 0x000000ffU}};
    EXPECT_EQ(smoothMathRun(formula, formula.runs[0], 0.1F).vertices.size(), 8U);
}

TEST(MathSmoothing, RejectsAnInvalidPixelSize) {
    const MathFormula square = unitSquare();
    EXPECT_THROW((void)smoothMathRun(square, square.runs[0], 0.0F), std::invalid_argument);
}

TEST(MathItem, ZoomingInRequestsFinerCurves) {
    MathItem item;
    QSignalSpy finished(&item, &MathItem::formulaChanged);
    item.setTex(QStringLiteral("\\sqrt{x}"));
    ASSERT_TRUE(finished.wait(waitMs));
    const int triangles = item.triangleCount();
    item.setViewScale(8.0);
    ASSERT_TRUE(finished.wait(waitMs));
    EXPECT_GT(item.triangleCount(), triangles);
}

TEST(MathItem, SizesItselfToTheFormulaAndKeepsItOnError) {
    MathItem item;
    QSignalSpy finished(&item, &MathItem::formulaChanged);
    item.setTex(QStringLiteral("x^2"));
    ASSERT_TRUE(finished.wait(waitMs));
    const qreal width = item.implicitWidth();
    EXPECT_GT(width, 0.0);
    EXPECT_GT(item.triangleCount(), 0);
    item.setFontSize(item.fontSize() * 2.0);
    EXPECT_DOUBLE_EQ(item.implicitWidth(), width * 2.0);
    item.setTex(QStringLiteral("\\frac{"));
    ASSERT_TRUE(finished.wait(waitMs));
    EXPECT_FALSE(item.error().isEmpty());
    EXPECT_DOUBLE_EQ(item.implicitWidth(), width * 2.0);
}

} // namespace
} // namespace peerchalk

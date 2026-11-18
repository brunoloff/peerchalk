#include "math/math_engine.hpp"

#include <peerchalk_math.h>

#include <QFuture>
#include <QPromise>
#include <QThreadPool>

#include <memory>
#include <stdexcept>

namespace peerchalk {
namespace {

struct MeshDeleter {
    void operator()(PeerChalkMathMesh *mesh) const { peerchalk_math_free(mesh); }
};

MathFormula computeFormula(const MathRequest &request) {
    const QByteArray tex = request.tex.toUtf8();
    const std::unique_ptr<PeerChalkMathMesh, MeshDeleter> mesh(
        peerchalk_math_layout(tex.constData(), std::size_t(tex.size()), request.display ? 1 : 0,
                              toMathRgba(request.color), request.tolerance));
    MathFormula formula;
    if (mesh->error != nullptr) {
        formula.error = QString::fromUtf8(mesh->error);
        return formula;
    }
    formula.vertices.assign(mesh->vertices, mesh->vertices + mesh->vertex_count * 2);
    formula.indices.assign(mesh->indices, mesh->indices + mesh->index_count);
    formula.runs.reserve(mesh->run_count);
    for (std::size_t i = 0; i < mesh->run_count; ++i) {
        const PeerChalkMathRun &run = mesh->runs[i];
        formula.runs.push_back({run.first_index, run.index_count, run.rgba});
    }
    formula.width = mesh->width;
    formula.height = mesh->height;
    formula.depth = mesh->depth;
    return formula;
}

} // namespace

void layoutMath(const MathRequest &request, QObject *receiver, MathCallback done) {
    if (receiver == nullptr || !done)
        throw std::invalid_argument("layoutMath needs a receiver and a callback");
    auto promise = std::make_shared<QPromise<MathFormula>>();
    promise->start();
    (void)promise->future().then(receiver, std::move(done));
    QThreadPool::globalInstance()->start([promise, request] {
        promise->addResult(computeFormula(request));
        promise->finish();
    });
}

} // namespace peerchalk

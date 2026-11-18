#include "math/math_engine.hpp"

#include <QPointer>

#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>

#include <map>
#include <stdexcept>
#include <utility>

namespace {

struct PendingFormula {
    QPointer<QObject> receiver;
    peerchalk::MathCallback done;
};

std::map<int, PendingFormula> pendingFormulas;
QByteArray resultBuffer;
int nextRequest = 0;

EM_JS(void, peerchalk_math_request,
      (int id, const char *tex, unsigned size, int display, unsigned rgba, float tolerance),
      { peerchalkMathRequest(id, tex, size, display, rgba, tolerance); });

} // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE char *peerchalk_math_result_buffer(unsigned size) {
    resultBuffer.resize(static_cast<qsizetype>(size));
    return resultBuffer.data();
}

EMSCRIPTEN_KEEPALIVE void peerchalk_math_result_ready(int id) {
    const auto found = pendingFormulas.find(id);
    if (found == pendingFormulas.end())
        return;
    PendingFormula pending = std::move(found->second);
    pendingFormulas.erase(found);
    if (pending.receiver)
        pending.done(peerchalk::parseMathResult(resultBuffer));
}
}

namespace peerchalk {

void layoutMath(const MathRequest &request, QObject *receiver, MathCallback done) {
    if (receiver == nullptr || !done)
        throw std::invalid_argument("layoutMath needs a receiver and a callback");
    const int id = nextRequest++;
    pendingFormulas.emplace(id, PendingFormula{receiver, std::move(done)});
    const QByteArray tex = request.tex.toUtf8();
    peerchalk_math_request(id, tex.constData(), static_cast<unsigned>(tex.size()),
                           request.display ? 1 : 0, toMathRgba(request.color), request.tolerance);
}

} // namespace peerchalk

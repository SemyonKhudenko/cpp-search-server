#include "log_duration.h"

using namespace std;

LogDuration::LogDuration(const string_view id, ostream& out)
    : id_(id)
    , out_(out) {
}

LogDuration::~LogDuration() {
    using namespace chrono;
    using namespace literals;

    const auto end_time = Clock::now();
    const auto dur = end_time - start_time_;
    out_ << id_ << ": "s << duration_cast<milliseconds>(dur).count() << " ms"s << endl;
}

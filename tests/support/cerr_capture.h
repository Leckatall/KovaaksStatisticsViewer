#ifndef KOVAAKSSTATSVIEWER_TESTS_CERR_CAPTURE_H
#define KOVAAKSSTATSVIEWER_TESTS_CERR_CAPTURE_H

#include <iostream>
#include <sstream>
#include <string>

namespace ksv::tests_support {
    // RAII redirect of std::cerr so diagnostic-only logging can be asserted on.
    class CerrCapture {
    public:
        CerrCapture() : m_previous(std::cerr.rdbuf(m_sink.rdbuf())) {}
        ~CerrCapture() { std::cerr.rdbuf(m_previous); }
        [[nodiscard]] std::string str() const { return m_sink.str(); }

    private:
        std::ostringstream m_sink;
        std::streambuf *m_previous;
    };
}

#endif

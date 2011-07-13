#include "fsigc++.h"

namespace fsigc {

connection::connection(mysignal_base *sig, void *func) {
    m_functor = func;
    m_sig = sig;
    m_connected = true;
}

void connection::disconnect() {
    if (m_connected) {
        m_sig->disconnect(m_functor);
        m_connected = false;
    }
}

}

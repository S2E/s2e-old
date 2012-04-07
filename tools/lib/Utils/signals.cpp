#include "Signals/fsigc++.h"

namespace fsigc {

connection::connection(mysignal_base *sig, void *func, unsigned index) {
    m_functor = func;
    m_sig = sig;
    m_connected = true;
    m_index = index;
}

void connection::disconnect() {
    if (m_connected) {
        m_sig->disconnect(m_functor, m_index);
        m_connected = false;
    }
}

}

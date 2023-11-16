
#include "noc_vc.h"

noc_vc::noc_vc(bool is_dummy)
    : _head(0), _tail(0), _fifo_buf(nullptr) {
    if (!is_dummy) {
        _fifo_buf = new noc_vc_fifo_t[NOC_VC_BUF_SIZE];
    }
}

bool noc_vc::enqueue(noc_data_t data, noc_link_ctrl_t link_ctrl) {
    // reject if full
    if ((_tail - _head) == NOC_VC_BUF_SIZE) return false;

    // add at tail
    _fifo_buf[_tail & NOC_VC_PTR_MASK].data = data;
    _fifo_buf[_tail & NOC_VC_PTR_MASK].link_ctrl = link_ctrl;

    // increment pointer
    _tail++;

    return true;
}

bool noc_vc::dequeue(noc_data_t& data, noc_link_ctrl_t& link_ctrl) {
    // reject if empty
    if (_tail == _head) return false;

    // read head
    data = _fifo_buf[_head & NOC_VC_PTR_MASK].data;
    link_ctrl = _fifo_buf[_head & NOC_VC_PTR_MASK].link_ctrl;

    // increment pointer
    _head++;

    return true;
}

bool noc_vc::peek(noc_data_t& data, noc_link_ctrl_t& link_ctrl) {
    // reject if empty
    if (_tail == _head) return false;

    // read head
    data = _fifo_buf[_head & NOC_VC_PTR_MASK].data;
    link_ctrl = _fifo_buf[_head & NOC_VC_PTR_MASK].link_ctrl;

    return true;
}

#include <string>

#include "systemc.h"

#include "checksum.hpp"
#include "system.h"
#include "noc_tile.h"

noc_commander::noc_commander(sc_module_name name) : noc_tile(name) {
    SC_THREAD(main);
    SC_THREAD(recv_listener);

    // read buffers
    _write_buf_size = read_input_files(_write_buf);
    _exp_buf_size = read_expected_output_file(_exp_buf);

    // print buffers
    int i = 0;
    printf(  "Encryption key:  ");
    for (int j = 0; j < AES_256_KEY_LEN; ++j, ++i) {
        printf("%02x ", _write_buf[i]);
    }
    printf("\nIV:              ");
    for (int j = 0; j < AES_BLOCK_LEN; ++j, ++i) {
        printf("%02x ", _write_buf[i]);
    }
    printf("\nInput:           ");
    for (int j = 0; j < _write_buf_size - AES_BLOCK_LEN - AES_256_KEY_LEN; ++j, ++i) {
        printf("%02x ", _write_buf[i]);
    }
    printf("\nExpected output: ");
    for (int j = 0; j < _exp_buf_size; ++j) {
        printf("%02x ", _exp_buf[j]);
    }
    printf("\n");

    // initial state
    _state = NOC_COMMANDER_IDLE;
}

void noc_commander::transmit_to_responders(noc_data_t *packets, uint32_t n) {
    // determine number of packets
    n = (n / NOC_DSIZE) + ((n % NOC_DSIZE) ? 1 : 0);

    // interleave
    uint32_t noc_responder0_addr = NOC_BASE_ADDR_RESPONDER0;
    uint32_t noc_responder1_addr = NOC_BASE_ADDR_RESPONDER1;
    uint32_t noc_responder2_addr = NOC_BASE_ADDR_RESPONDER2;
    while (n) {
        adapter_if->write_packet(0, noc_responder0_addr, packets, sizeof(noc_data_t));
        adapter_if->write_packet(0, noc_responder1_addr, packets, sizeof(noc_data_t));
        adapter_if->write_packet(0, noc_responder2_addr, packets, sizeof(noc_data_t));

        // increment counters
        n--;
        noc_responder0_addr += NOC_DSIZE;
        noc_responder1_addr += NOC_DSIZE;
        noc_responder2_addr += NOC_DSIZE;
        packets++;
    }
}

void noc_commander::main() {
    LOG("Hello, world!");

    POSEDGE();

    // write command
    _state = NOC_COMMANDER_WRITE_DATA;
    _cur_cmd.skey = NOC_CMD_SKEY;
    _cur_cmd.cmd = 0;
    _cur_cmd.size = _write_buf_size;
    _cur_cmd.tx_addr = 0x0;
    _cur_cmd.trans_id = 1;
    _cur_cmd.status = 0;
    _cur_cmd.ekey = NOC_CMD_EKEY;
    _cur_cmd.chksum = CALC_CMD_CHKSUM(_cur_cmd);
    transmit_to_responders((noc_data_t *)&_cur_cmd, sizeof(noc_cmd_t));

    // write payload
    transmit_to_responders((noc_data_t *)_write_buf, _write_buf_size);
}

void noc_commander::recv_listener() {
    // NoC packets
    uint32_t src_addr;
    uint32_t rel_addr;
    noc_data_t data;

    // buffers
    uint32_t tot_cursor = 0;
    uint8_t rsp_buf[MAX_OUT_SIZE];

    // keep track of redundant communications
    int32_t redundant_src_idx;
    uint32_t checkpoint_size = 4; // checkpoints every 4 packets (32B)
    tmr_packet_status_e status;
    noc_data_t *out_cursor = (noc_data_t*)rsp_buf;
    tmr_state_collection<noc_data_t> states(checkpoint_size, MAX_OUT_SIZE / sizeof(noc_data_t) / checkpoint_size);

    while (true) {
        // receive packet
        if (adapter_if->read_packet(src_addr, rel_addr, data)) {
            LOGF("[%s]: received request containing %016lx to %08x from %08x", this->name(), data, rel_addr, src_addr);

            // determine source
            switch (src_addr & NOC_ADDR_XY_MASK) {
            case NOC_BASE_ADDR_RESPONDER0: redundant_src_idx = 0; break;
            case NOC_BASE_ADDR_RESPONDER1: redundant_src_idx = 1; break;
            case NOC_BASE_ADDR_RESPONDER2: redundant_src_idx = 2; break;
            default: redundant_src_idx = -1; break;
            };

            // update CRC
            status = states.update(redundant_src_idx, data, out_cursor);
            if (status == TMR_STATUS_COMMIT) {
                LOGF("Majority reached for byte 0x%x\n", tot_cursor);
                // update cursors after result commit
                out_cursor += checkpoint_size;
                tot_cursor += NOC_DSIZE * checkpoint_size;

                if (tot_cursor >= _exp_buf_size) {
                    LOG("Setting to IDLE");
                    _state == NOC_COMMANDER_IDLE;
                    break;
                }
            }
            else if (status == TMR_STATUS_INVALID) {
                LOGF("Invalid vote, no majority reached at byte 0x%x\n", tot_cursor);
                break;
            }
        }
    }

    // compare received and expected buffer
    LOG("Completed simulation, checking output...");
    uint32_t n_bytes_cmp = 0;
    uint32_t n_err_bytes = 0;
    for (int i = 0; i < MAX_OUT_SIZE; ++i) {
        if (rsp_buf[i] != _exp_buf[i]) {
            //LOGF("Unexpected output at byte %d, expected %02x, received %02x", i, _exp_buf[i], rsp_buf[i]);
            n_err_bytes++;
        }
        n_bytes_cmp++;
    }
    printf("Final report: %d bytes compared, %d errors\n", n_bytes_cmp, n_err_bytes);

    sc_stop();
}


#include "system.h"
#include "sc_trace.hpp"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>

char *key_file_name = (char*)"../key.bin";
char *iv_file_name = (char*)"../iv.bin";
char *in_file_name = (char*)"../in.bin";
char *expected_out_file_name = (char*)"../out.bin";

bool parse_cmd_line(int argc, char **argv) {
    // check usage
    if (argc < 1 || argc > 6) {
        std::cerr << "Usage: " << argv[0] << " [<KEY_FILE> <IV_FILE> <IN_FILE> <OUT_FILE>] [<TRACE_FILE>]" << std::endl;
        return false;
    }

    int argi = 1;

    if (argc >= 5) {
        // latch file names
        key_file_name = argv[argi++];
        iv_file_name = argv[argi++];
        in_file_name = argv[argi++];
        expected_out_file_name = argv[argi++];
    }

    if (argi < argc) {
        sc_tracer::enable();
        sc_tracer::init(argv[argi++]);
    }
    else {
        sc_tracer::disable();
    }

    return true;
}

uint32_t read_input_files(uint8_t *out) {
    uint32_t n = 0;
    
    // read key file
    FILE *fp = fopen(key_file_name, "rb");
    if (fp) {
        n += fread(out + n, 1, 32, fp);
        fclose(fp);
    }
    else {
        return 0;
    }
    
    // read IV file
    fp = fopen(iv_file_name, "rb");
    if (fp) {
        n += fread(out + n, 1, 16, fp);
        fclose(fp);
    }
    else {
        return 0;
    }
    
    // read input file
    fp = fopen(in_file_name, "rb");
    if (fp) {
        // get file size
        fseek(fp, 0L, SEEK_END);
        uint32_t input_size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        
        if (input_size > MAX_DATA_SIZE) {
            input_size = MAX_DATA_SIZE;
        }
        
        n += fread(out + n, 1, input_size, fp);
        fclose(fp);
    }
    else {
        return 0;
    }
    
    return n;
}

uint32_t read_expected_output_file(uint8_t *out) {
    uint32_t n = 0;
    
    // read output file
    FILE *fp = fopen(expected_out_file_name, "rb");
    if (fp) {
        // get file size
        fseek(fp, 0L, SEEK_END);
        uint32_t output_size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        
        if (output_size > MAX_OUT_SIZE) {
            output_size = MAX_OUT_SIZE;
        }
        
        n += fread(out + n, 1, output_size, fp);
        fclose(fp);
    }
    else {
        return 0;
    }
    
    return n;
}

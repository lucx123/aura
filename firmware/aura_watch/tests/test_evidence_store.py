"""Production journal logic with a fault-injected NVS model; no physical flash."""
import argparse
import _ctypes
import ctypes as C
import os
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
ERRORS = '''#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_NVS_NOT_FOUND 0x1102
'''
NVS = '''#pragma once
#include <stddef.h>
#include "esp_err.h"
typedef unsigned nvs_handle_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
esp_err_t nvs_open_from_partition(const char *,const char *,int,nvs_handle_t *);
esp_err_t nvs_get_blob(nvs_handle_t,const char *,void *,size_t *);
esp_err_t nvs_set_blob(nvs_handle_t,const char *,const void *,size_t);
esp_err_t nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
'''
STUB = '''#include "nvs.h"
#include <string.h>
static unsigned char saved[2048],pending[2048];
static size_t size,pending_size;
static int exists,active,fault,committed;
void model_reset(void){size=0;pending_size=0;exists=active=fault=committed=0;}
void model_fault(int value){fault=value;committed=0;}
int model_active(void){return active;}
void model_corrupt(unsigned offset,unsigned value){if(offset<size)saved[offset]=(unsigned char)value;}
esp_err_t nvs_open_from_partition(const char *p,const char *ns,int mode,nvs_handle_t *h){
 if(strcmp(p,"aura_cfg")||strcmp(ns,"evidence")||fault==1)return ESP_FAIL;
 if(!exists&&mode==NVS_READONLY)return ESP_ERR_NVS_NOT_FOUND;
 *h=1;++active;return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n){
 if(!h||strcmp(key,"journal")||fault==2)return ESP_FAIL;
 if(!exists)return ESP_ERR_NVS_NOT_FOUND;
 if(*n<size){*n=size;return ESP_ERR_INVALID_SIZE;}
 memcpy(out,saved,size);*n=size;
 if(fault==5&&committed)((unsigned char *)out)[16]^=1;
 if(fault==6)*n=size-1;
 return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t n){
 if(!h||strcmp(key,"journal")||n>sizeof(pending)||fault==3)return ESP_FAIL;
 memcpy(pending,data,n);pending_size=n;return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h){
 if(!h||fault==4)return ESP_FAIL;
 memcpy(saved,pending,pending_size);size=pending_size;exists=committed=1;return ESP_OK;
}
void nvs_close(nvs_handle_t h){if(h)--active;}
'''


class Status(C.Structure):
    _fields_ = [('sequence', C.c_uint32), ('count', C.c_uint8),
                ('verified', C.c_bool), ('error', C.c_int)]


class EvidenceTests(unittest.TestCase):
    def setUp(self): self.lib.model_reset()
    def tearDown(self): self.assertEqual(self.lib.model_active(), 0, 'Leaked NVS handle')
    def append(self, line):
        status = Status()
        result = self.lib.aura_evidence_store_append(line, C.byref(status))
        self.assertEqual(result, status.error)
        return result, status
    def read(self, index, capacity=192):
        output = C.create_string_buffer(capacity)
        error = self.lib.aura_evidence_store_read(index, output, capacity)
        return error, output.value
    def test_empty_store_is_available_without_creation(self):
        status = Status()
        self.assertEqual(self.lib.aura_evidence_store_status(C.byref(status)), 0)
        self.assertEqual((status.sequence, status.count, status.verified), (0, 0, False))
    def test_append_readback_and_reopen(self):
        result, status = self.append(b'{"event":"manual","time":123}')
        self.assertEqual(result, 0)
        self.assertEqual((status.sequence, status.count, status.verified), (1, 1, True))
        self.assertEqual(self.read(0), (0, b'{"event":"manual","time":123}'))
        reopened = Status()
        self.assertEqual(self.lib.aura_evidence_store_status(C.byref(reopened)), 0)
        self.assertEqual((reopened.sequence, reopened.count, reopened.verified), (1, 1, False))
    def test_wrap_retains_newest_eight_in_order(self):
        for i in range(25):
            error, status = self.append(f'{{"record":{i}}}'.encode())
            self.assertEqual(error, 0)
        self.assertEqual((status.sequence, status.count), (25, 8))
        for i in range(8): self.assertEqual(self.read(i), (0, f'{{"record":{i+17}}}'.encode()))
        self.assertEqual(self.read(8)[0], 0x105)
    def test_bounds_and_invalid_lines_preserve_old_record(self):
        self.assertEqual(self.append(b'original')[0], 0)
        for line in (None, b'', b'x'*192, b'bad\nline', b'bad\rline'):
            error, status = self.append(line)
            self.assertEqual(error, 0x102); self.assertFalse(status.verified)
            self.assertEqual(self.read(0), (0, b'original'))
        self.assertEqual(self.append(b'x'*191)[0], 0)
        self.assertEqual(self.read(1, 191)[0], 0x104)
        self.assertEqual(self.read(1), (0, b'x'*191))
    def test_failures_never_claim_verification(self):
        for fault in (1, 2, 3, 4, 5, 6):
            self.lib.model_reset(); self.append(b'original'); self.lib.model_fault(fault)
            error, status = self.append(b'new')
            self.assertNotEqual(error, 0); self.assertFalse(status.verified)
            self.assertEqual(self.lib.model_active(), 0)
    def test_set_and_commit_failure_preserve_existing_journal(self):
        for fault in (3, 4):
            self.lib.model_reset(); self.append(b'original'); self.lib.model_fault(fault)
            self.assertNotEqual(self.append(b'new')[0], 0)
            self.lib.model_fault(0)
            self.assertEqual(self.read(0), (0, b'original'))
            self.assertEqual(self.read(1)[0], 0x105)
    def test_malformed_metadata_is_preserved_and_rejected(self):
        for offset, value in ((0, 0), (4, 2), (6, 9), (8, 8), (10, 1), (12, 0), (16, 0)):
            self.lib.model_reset(); self.append(b'original'); self.lib.model_corrupt(offset,value)
            self.assertEqual(self.append(b'new')[0], 0x103)
            self.assertEqual(self.read(0)[0], 0x103)
    def test_sequence_overflow_rejects_write(self):
        self.append(b'original')
        for offset in range(12,16): self.lib.model_corrupt(offset,255)
        self.assertEqual(self.append(b'new')[0], 0x106)
        self.assertEqual(self.read(0), (0,b'original'))
    def test_export_is_one_ordered_snapshot(self):
        for i in range(12): self.append(str(i).encode())
        rows = (C.c_char * 192 * 8)(); count = C.c_uint()
        self.assertEqual(self.lib.aura_evidence_store_copy(rows,C.byref(count)),0)
        self.assertEqual(count.value,8)
        self.assertEqual([bytes(row).split(b'\0',1)[0] for row in rows],
                         [str(i).encode() for i in range(4,12)])
        self.append(b'next')
        self.assertEqual(bytes(rows[0]).split(b'\0',1)[0],b'4')
        self.lib.model_fault(2); count.value=99
        self.assertNotEqual(self.lib.aura_evidence_store_copy(rows,C.byref(count)),0)
        self.assertEqual(count.value,0)


def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--cc', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='aura-evidence-test-') as directory:
        directory = pathlib.Path(directory)
        for name, source in (('esp_err.h', ERRORS), ('nvs.h', NVS), ('model.c', STUB)):
            (directory/name).write_text(source)
        target = directory/'evidence.dll'
        command = [args.cc] + (['cc'] if pathlib.Path(args.cc).name.startswith('zig') else [])
        command += ['-std=c11','-O2','-Wall','-Wextra','-Werror','-shared',
                    '-I',str(directory),'-I',str(ROOT/'main'),
                    str(ROOT/'main/aura_evidence_store.c'),str(directory/'model.c'),'-o',str(target)]
        subprocess.run(command, check=True)
        lib = C.CDLL(str(target)); EvidenceTests.lib = lib
        lib.aura_evidence_store_append.argtypes = [C.c_char_p,C.POINTER(Status)]
        lib.aura_evidence_store_status.argtypes = [C.POINTER(Status)]
        lib.aura_evidence_store_read.argtypes = [C.c_uint,C.c_void_p,C.c_size_t]
        try:
            result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(EvidenceTests))
        finally:
            if os.name == 'nt': _ctypes.FreeLibrary(lib._handle)
            else: _ctypes.dlclose(lib._handle)
        if not result.wasSuccessful(): raise SystemExit(1)


if __name__ == '__main__': main()

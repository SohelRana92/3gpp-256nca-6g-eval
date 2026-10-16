#!/usr/bin/env python3
"""Cross-check the C reference against independent implementations.

AES-256:  compared with the `cryptography` library (AES in ECB mode) on
          random keys and blocks.
POLYVAL:  a pure-Python POLYVAL is first validated against the library's
          AES-256-GCM. GHASH is recovered from a GCM tag and compared with
          POLYVAL through the identity of RFC 8452, Appendix A. When the
          library also offers AESGCMSIV (cryptography >= 42), full
          AES-256-GCM-SIV is checked as well. The C POLYVAL is then compared
          with the validated Python POLYVAL on random inputs.

Usage: python3 verify_common.py ./test_common [trials]
"""
import os
import subprocess
import sys

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

try:  # AESGCMSIV exists only in cryptography >= 42
    from cryptography.hazmat.primitives.ciphers.aead import AESGCMSIV
except ImportError:
    AESGCMSIV = None

BIN = sys.argv[1] if len(sys.argv) > 1 else "./test_common"
TRIALS = int(sys.argv[2]) if len(sys.argv) > 2 else 200

P = (1 << 128) | (1 << 127) | (1 << 126) | (1 << 121) | 1


def gf_mul(a, b):
    """Plain product in GF(2)[x] / p(x)."""
    r = 0
    while b:
        if b & 1:
            r ^= a
        b >>= 1
        a <<= 1
        if a >> 128:
            a ^= P
    return r


def gf_inv_x128():
    # x^-1 = x^127 + x^126 + x^125 + x^120 (since x * that = p - 1 = 1).
    xinv = (1 << 127) | (1 << 126) | (1 << 125) | (1 << 120)
    r = 1
    for _ in range(128):
        r = gf_mul(r, xinv)
    return r


X_INV_128 = gf_inv_x128()


def dot(a, b):
    return gf_mul(gf_mul(a, b), X_INV_128)


def py_polyval(h: bytes, x: bytes) -> bytes:
    hh = int.from_bytes(h, "little")
    s = 0
    for i in range(0, len(x), 16):
        s = dot(s ^ int.from_bytes(x[i:i + 16], "little"), hh)
    return s.to_bytes(16, "little")


def aes_ecb(key: bytes, block: bytes) -> bytes:
    e = Cipher(algorithms.AES(key), modes.ECB()).encryptor()
    return e.update(block) + e.finalize()


def py_gcmsiv_encrypt(key: bytes, nonce: bytes, pt: bytes, aad: bytes) -> bytes:
    """AES-256-GCM-SIV per RFC 8452 Section 4, using py_polyval."""
    def blk(i):
        return aes_ecb(key, i.to_bytes(4, "little") + nonce)[:8]
    mac_key = blk(0) + blk(1)
    enc_key = blk(2) + blk(3) + blk(4) + blk(5)

    def pad(b):
        return b + b"\x00" * (-len(b) % 16)
    length_block = (len(aad) * 8).to_bytes(8, "little") + (len(pt) * 8).to_bytes(8, "little")
    s = bytearray(py_polyval(mac_key, pad(aad) + pad(pt) + length_block))
    for i in range(12):
        s[i] ^= nonce[i]
    s[15] &= 0x7F
    tag = aes_ecb(enc_key, bytes(s))
    ctr = bytearray(tag)
    ctr[15] |= 0x80
    out = bytearray()
    c = int.from_bytes(ctr[:4], "little")
    for i in range(0, len(pt), 16):
        ks = aes_ecb(enc_key, c.to_bytes(4, "little") + bytes(ctr[4:]))
        out += bytes(a ^ b for a, b in zip(pt[i:i + 16], ks))
        c = (c + 1) & 0xFFFFFFFF
    return bytes(out) + tag


def ghash_via_polyval(h: bytes, x: bytes) -> bytes:
    """GHASH(H, X) = ByteReverse(POLYVAL(mulX_POLYVAL(ByteReverse(H)), ByteReverse(X_i)...)), RFC 8452 App. A."""
    hp = gf_mul(int.from_bytes(h[::-1], "little"), 2).to_bytes(16, "little")
    xr = b"".join(x[i:i + 16][::-1] for i in range(0, len(x), 16))
    return py_polyval(hp, xr)[::-1]


def ghash_from_gcm(key: bytes, nonce: bytes, aad: bytes) -> bytes:
    """GHASH(H, AAD || lengths) recovered from an AES-256-GCM tag with empty plaintext."""
    tag = AESGCM(key).encrypt(nonce, b"", aad)
    ekj0 = aes_ecb(key, nonce + b"\x00\x00\x00\x01")
    return bytes(a ^ b for a, b in zip(tag, ekj0))


def run(*args):
    return subprocess.run([BIN, *args], capture_output=True, text=True, check=True).stdout.strip()


def main():
    fails = 0

    # 1a. Validate the Python POLYVAL through AES-256-GCM (works on any library version).
    for t in range(TRIALS):
        key, nonce = os.urandom(32), os.urandom(12)
        aad = os.urandom(16 * (t % 9))
        h = aes_ecb(key, bytes(16))
        x = aad + (len(aad) * 8).to_bytes(8, "big") + bytes(8)
        if ghash_via_polyval(h, x) != ghash_from_gcm(key, nonce, aad):
            fails += 1
    print(f"Python POLYVAL via AES-256-GCM vs cryptography:     {TRIALS - fails}/{TRIALS} match")

    # 1b. Validate it through full AES-256-GCM-SIV, if the library has it.
    if AESGCMSIV is not None:
        f1 = 0
        for t in range(TRIALS):
            key, nonce = os.urandom(32), os.urandom(12)
            pt, aad = os.urandom(1 + t % 70), os.urandom((t * 7) % 50)   # cryptography >= 43 rejects empty GCM-SIV data
            if py_gcmsiv_encrypt(key, nonce, pt, aad) != AESGCMSIV(key).encrypt(nonce, pt, aad):
                f1 += 1
        print(f"Python POLYVAL via AES-256-GCM-SIV vs cryptography: {TRIALS - f1}/{TRIALS} match")
        fails += f1
    else:
        print("AES-256-GCM-SIV check skipped (cryptography < 42); the GCM check above covers POLYVAL")
    py_ok = fails == 0

    # 2. C AES-256 vs cryptography.
    f2 = 0
    for _ in range(TRIALS):
        k, p = os.urandom(32), os.urandom(16)
        if run("aes", k.hex(), p.hex()) != aes_ecb(k, p).hex():
            f2 += 1
    print(f"C AES-256 vs cryptography AES-256:                  {TRIALS - f2}/{TRIALS} match")

    # 3. C POLYVAL vs validated Python POLYVAL.
    f3 = 0
    for t in range(TRIALS):
        h, x = os.urandom(16), os.urandom(16 * (1 + t % 8))
        if run("pv", h.hex(), x.hex()) != py_polyval(h, x).hex():
            f3 += 1
    print(f"C POLYVAL vs validated Python POLYVAL:              {TRIALS - f3}/{TRIALS} match")

    ok = py_ok and f2 == 0 and f3 == 0
    print("ALL CROSS-CHECKS PASS" if ok else "CROSS-CHECK FAILURES")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()

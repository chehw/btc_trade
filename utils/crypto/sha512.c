/*
 * sha512.c
 * 
 * Copyright 2019 Che Hongwei <htc.chehw@gmail.com>
 * 
 * The MIT License (MIT)
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, merge, 
 * publish, distribute, sublicense, and/or sell copies of the Software, 
 * and to permit persons to whom the Software is furnished to do so, 
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 *  in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR 
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 */


/*
 * sha256.c
 * origin: https://github.com/bitcoin/bitcoin/blob/master/src/crypto/sha256.cpp
 * modified by: Che Hongwei <htc.chehw@gmail.com>
 * 
 * The MIT License (MIT)
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "sha.h"

#include <endian.h>


/**
 * RFC 6234 US Secure Hash Algorithms  (SHA and SHA-based HMAC and HKDF)
 * link: https://datatracker.ietf.org/doc/html/rfc6234#section-5.2
*/ 

#define SHA512_BLOCK_SIZE 128

// SHA-512 initial hash values (in big-endian):
static uint64_t s_init_values[8] = {
	0x6a09e667f3bcc908, 
	0xbb67ae8584caa73b, 
	0x3c6ef372fe94f82b, 
	0xa54ff53a5f1d36f1, 
	0x510e527fade682d1, 
	0x9b05688c2b3e6c1f, 
	0x1f83d9abfb41bd6b, 
	0x5be0cd19137e2179
};

// SHA-512 round constants:
static uint64_t K[80] = {
	0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc, 0x3956c25bf348b538, 
	0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118, 0xd807aa98a3030242, 0x12835b0145706fbe, 
	0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2, 0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 
	0xc19bf174cf692694, 0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65, 
	0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5, 0x983e5152ee66dfab, 
	0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4, 0xc6e00bf33da88fc2, 0xd5a79147930aa725, 
	0x06ca6351e003826f, 0x142929670a0e6e70, 0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 
	0x53380d139d95b3df, 0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b, 
	0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30, 0xd192e819d6ef5218, 
	0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8, 0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 
	0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8, 0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 
	0x682e6ff3d6b2b8a3, 0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec, 
	0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b, 0xca273eceea26619c, 
	0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178, 0x06f067aa72176fba, 0x0a637dc5a2c898a6, 
	0x113f9804bef90dae, 0x1b710b35131c471b, 0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 
	0x431d67c49c100d4c, 0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
};

/*
 * SHA-512 Sum & Sigma:
 * S0 := (a rightrotate 28) xor (a rightrotate 34) xor (a rightrotate 39)
 * S1 := (e rightrotate 14) xor (e rightrotate 18) xor (e rightrotate 41)
 * s0 := (w[i-15] rightrotate 1) xor (w[i-15] rightrotate 8) xor (w[i-15] rightshift 7)
 * s1 := (w[i-2] rightrotate 19) xor (w[i-2] rightrotate 61) xor (w[i-2] rightshift 6)
*/
/*
 * SHA-384 and SHA-512 each use six logical functions, where each
 * function operates on 64-bit words, which are represented as x, y, and
 * z.  The result of each function is a new 64-bit word.
 * 
 * CH( x, y, z) = (x AND y) XOR ( (NOT x) AND z)
 * MAJ( x, y, z) = (x AND y) XOR (x AND z) XOR (y AND z)
 * BSIG0(x) = ROTR^28(x) XOR ROTR^34(x) XOR ROTR^39(x)
 * BSIG1(x) = ROTR^14(x) XOR ROTR^18(x) XOR ROTR^41(x)
 * SSIG0(x) = ROTR^1(x) XOR ROTR^8(x) XOR SHR^7(x)
 * SSIG1(x) = ROTR^19(x) XOR ROTR^61(x) XOR SHR^6(x)
*/

#define CH(x, y, z) ( ( ((uint64_t)(x)) & ((uint64_t)(y)) ) ^ ( (~((uint64_t)(x))) & ((uint64_t)(z)) ) )
#define MAJ(x, y, z) ( ( ((uint64_t)(x)) & ((uint64_t)(y)) ) ^ ( ((uint64_t)(x)) & ((uint64_t)(z)) ) ^ ( ((uint64_t)(y)) & ((uint64_t)(z)) )  )
//~ #define BSIG0(x) ( (((uint64_t)(x)) >> 28) ^ (((uint64_t)(x)) >> 34) ^ (((uint64_t)(x)) >> 39) )
#define BSIG1(x) ( (((uint64_t)(x)) >> 14) ^ (((uint64_t)(x)) >> 18) ^ (((uint64_t)(x)) >> 41) )
#define SSIG0(x) ( (((uint64_t)(x)) >> 1) ^ (((uint64_t)(x)) >> 8) ^ (((uint64_t)(x)) >> 7) )
#define SSIG1(x) ( (((uint64_t)(x)) >> 19) ^ (((uint64_t)(x)) >> 61) ^ (((uint64_t)(x)) >> 6) )

static inline uint64_t BSIG0(uint64_t x)
{
	uint64_t a = x >> 28;
	uint64_t b = x >> 34;
	uint64_t c = x >> 39;
	return (a ^ b ^ c);
}

void sha512_init(sha512_ctx_t * sha)
{
	memset(sha, 0, sizeof(*sha));
	memcpy(sha->s, s_init_values, sizeof(sha->s));
	//~ printf("\e[33m" "%s(%d)::%s()" "\e[39m" "\n", __FILE__, __LINE__, __FUNCTION__);
}

static inline void update_block(sha512_ctx_t * sha, const unsigned char * block)
{
	const uint64_t * M = (const uint64_t *)block; // message
	uint64_t a, b, c, d, e, f, g, h; // the eight working variables
	uint64_t W[80];	// the message schedule, 80 rounds
	
	/* 
	1. Prepare the message schedule W:
	 For t = 0 to 15
		Wt = M(i)t
	 For t = 16 to 79
		Wt = SSIG1(W(t-2)) + W(t-7) + SSIG0(W(t-15)) + W(t-16)
	*/
	for(int t = 0; t < 16; ++t) {
		W[t] = be64toh(M[t]); 	// convert big-endian message to the host byte-order
	}
	for(int t = 16; t < 80; ++t) {
		W[t] = SSIG1(W[t-2]) + W[t-7] + SSIG0(W[t-15]) + W[t-16];
	}
	
	/*
	2. Initialize the working variables:
		a = H(i-1)0
		b = H(i-1)1
		c = H(i-1)2
		d = H(i-1)3
		e = H(i-1)4
		f = H(i-1)5
		g = H(i-1)6
		h = H(i-1)7
    */
	a = sha->s[0];
	b = sha->s[1];
	c = sha->s[2];
	d = sha->s[3];
	e = sha->s[4];
	f = sha->s[5];
	g = sha->s[6];
	h = sha->s[7];
	
	/*
	3. Perform the main hash computation:
	 For t = 0 to 79
		T1 = h + BSIG1(e) + CH(e,f,g) + Kt + Wt
		T2 = BSIG0(a) + MAJ(a,b,c)
		h = g
		g = f
		f = e
		e = d + T1
		d = c
		c = b
		b = a
		a = T1 + T2
	*/
	for(int t = 0; t < 80; ++t) {
		uint64_t T1 = h + BSIG1(e) + CH(e,f,g) + K[t] + W[t];
		uint64_t T2 = BSIG0(a) + MAJ(a,b,c);
		h = g;
		g = f;
		f = e;
		e = d + T1;
		d = c;
		c = b;
		b = a;
		a = T1 + T2;
	}
	
	/*
	 4. Compute the intermediate hash value H(i)
		H(i)0 = a + H(i-1)0
		H(i)1 = b + H(i-1)1
		H(i)2 = c + H(i-1)2
		H(i)3 = d + H(i-1)3
		H(i)4 = e + H(i-1)4
		H(i)5 = f + H(i-1)5
		H(i)6 = g + H(i-1)6
		H(i)7 = h + H(i-1)7
	*/
	sha->s[0] += a;
	sha->s[1] += b;
	sha->s[2] += c;
	sha->s[3] += d;
	sha->s[4] += e;
	sha->s[5] += f;
	sha->s[6] += g;
	sha->s[7] += h;
	return;
}

void sha512_update(sha512_ctx_t * sha, const unsigned char * data, size_t len)
{
	const unsigned char * p_end = data + len;
	sha->total_bytes += len;
	
	if(sha->bytes_left) { // if has unprocessed data
		size_t length = sha->bytes_left + len;
		ssize_t offset = SHA512_BLOCK_SIZE - sha->bytes_left;
		memcpy(sha->buf, data, offset);
		
		sha->bytes_left += offset;
		if(length < SHA512_BLOCK_SIZE) return;
		
		update_block(sha, sha->buf);
		data += offset;
		len -= offset;
		sha->bytes_left = 0;
	}
	
	size_t num_blocks = len / SHA512_BLOCK_SIZE;
	for(size_t i = 0; i < num_blocks; ++i) 
	{
		update_block(sha, data);
		data += SHA512_BLOCK_SIZE;
		len -= SHA512_BLOCK_SIZE;
	}
	
	sha->bytes_left = p_end - data;
	if(sha->bytes_left > 0) {
		memcpy(sha->buf, data, sha->bytes_left);
	}
	return;
}

void sha512_final(sha512_ctx_t * sha, unsigned char _hash[static 64])
{
	// add paddings
	/*
	a. "1" is appended.  Example: if the original message is "01010000",
		this is padded to "010100001".

	b. K "0"s are appended where K is the smallest, non-negative solution
	to the equation
		( L + 1 + K ) mod 1024 = 896 ( == 112 bytes)
	*/	
	
	unsigned char * paddings = sha->buf;
	paddings[sha->bytes_left] = 0x80;
	if(sha->bytes_left >= 112) { // need two blocks to append paddings: '0x80' and the 16 bytes size_desc (total bits)
		update_block(sha, paddings);
		memset(paddings, 0, 128);
	}
	uint64_t total_bits = (uint64_t)sha->total_bytes << 3;
	*(uint64_t *)(paddings + 120) = htobe64(total_bits);
	update_block(sha, paddings);
	
	uint64_t * hash = (uint64_t *)_hash;
	for(int i = 0; i < 8; ++i) {
		hash[i] = htobe64(sha->s[i]);
	}
	return;
}





#if 0
static inline uint64_t Ch(uint64_t x, uint64_t y, uint64_t z) { return z ^ (x & (y ^ z)); }
static inline uint64_t Maj(uint64_t x, uint64_t y, uint64_t z) { return (x & y) | (z & (x | y)); }
static inline uint64_t Sigma0(uint64_t x) { return (x >> 28 | x << 36) ^ (x >> 34 | x << 30) ^ (x >> 39 | x << 25); }
static inline uint64_t Sigma1(uint64_t x) { return (x >> 14 | x << 50) ^ (x >> 18 | x << 46) ^ (x >> 41 | x << 23); }
static inline uint64_t sigma0(uint64_t x) { return (x >> 1 | x << 63) ^ (x >> 8 | x << 56) ^ (x >> 7); }
static inline uint64_t sigma1(uint64_t x) { return (x >> 19 | x << 45) ^ (x >> 61 | x << 3) ^ (x >> 6); }

static inline void Round(uint64_t a, uint64_t b, uint64_t c, 
	uint64_t *d, 
	uint64_t e, uint64_t f, uint64_t g, 
	uint64_t *h, 
	uint64_t k, uint64_t w)
{
    uint64_t t1 = *h + Sigma1(e) + Ch(e, f, g) + k + w;
    uint64_t t2 = Sigma0(a) + Maj(a, b, c);
    *d += t1;
    *h = t1 + t2;
}

/** Initialize SHA-512 state. */
static void inline Initialize(uint64_t * s)
{
    s[0] = 0x6a09e667f3bcc908ull;
    s[1] = 0xbb67ae8584caa73bull;
    s[2] = 0x3c6ef372fe94f82bull;
    s[3] = 0xa54ff53a5f1d36f1ull;
    s[4] = 0x510e527fade682d1ull;
    s[5] = 0x9b05688c2b3e6c1full;
    s[6] = 0x1f83d9abfb41bd6bull;
    s[7] = 0x5be0cd19137e2179ull;
}

/** Perform one SHA-512 transformation, processing a 64-byte chunk. */
static void Transform(uint64_t* s, const unsigned char* chunk)
{
    uint64_t a = s[0], b = s[1], c = s[2], d = s[3], e = s[4], f = s[5], g = s[6], h = s[7];
    uint64_t w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15;


    Round(a, b, c, &d, e, f, g, &h, 0x428a2f98d728ae22ull, w0 = ReadBE64(chunk + 0));
    Round(h, a, b, &c, d, e, f, &g, 0x7137449123ef65cdull, w1 = ReadBE64(chunk + 8));
    Round(g, h, a, &b, c, d, e, &f, 0xb5c0fbcfec4d3b2full, w2 = ReadBE64(chunk + 16));
    Round(f, g, h, &a, b, c, d, &e, 0xe9b5dba58189dbbcull, w3 = ReadBE64(chunk + 24));
    Round(e, f, g, &h, a, b, c, &d, 0x3956c25bf348b538ull, w4 = ReadBE64(chunk + 32));
    Round(d, e, f, &g, h, a, b, &c, 0x59f111f1b605d019ull, w5 = ReadBE64(chunk + 40));
    Round(c, d, e, &f, g, h, a, &b, 0x923f82a4af194f9bull, w6 = ReadBE64(chunk + 48));
    Round(b, c, d, &e, f, g, h, &a, 0xab1c5ed5da6d8118ull, w7 = ReadBE64(chunk + 56));
    Round(a, b, c, &d, e, f, g, &h, 0xd807aa98a3030242ull, w8 = ReadBE64(chunk + 64));
    Round(h, a, b, &c, d, e, f, &g, 0x12835b0145706fbeull, w9 = ReadBE64(chunk + 72));
    Round(g, h, a, &b, c, d, e, &f, 0x243185be4ee4b28cull, w10 = ReadBE64(chunk + 80));
    Round(f, g, h, &a, b, c, d, &e, 0x550c7dc3d5ffb4e2ull, w11 = ReadBE64(chunk + 88));
    Round(e, f, g, &h, a, b, c, &d, 0x72be5d74f27b896full, w12 = ReadBE64(chunk + 96));
    Round(d, e, f, &g, h, a, b, &c, 0x80deb1fe3b1696b1ull, w13 = ReadBE64(chunk + 104));
    Round(c, d, e, &f, g, h, a, &b, 0x9bdc06a725c71235ull, w14 = ReadBE64(chunk + 112));
    Round(b, c, d, &e, f, g, h, &a, 0xc19bf174cf692694ull, w15 = ReadBE64(chunk + 120));

    Round(a, b, c, &d, e, f, g, &h, 0xe49b69c19ef14ad2ull, w0 += sigma1(w14) + w9 + sigma0(w1));
    Round(h, a, b, &c, d, e, f, &g, 0xefbe4786384f25e3ull, w1 += sigma1(w15) + w10 + sigma0(w2));
    Round(g, h, a, &b, c, d, e, &f, 0x0fc19dc68b8cd5b5ull, w2 += sigma1(w0) + w11 + sigma0(w3));
    Round(f, g, h, &a, b, c, d, &e, 0x240ca1cc77ac9c65ull, w3 += sigma1(w1) + w12 + sigma0(w4));
    Round(e, f, g, &h, a, b, c, &d, 0x2de92c6f592b0275ull, w4 += sigma1(w2) + w13 + sigma0(w5));
    Round(d, e, f, &g, h, a, b, &c, 0x4a7489dcbd41fbd4ull, w6 += sigma1(w4) + w15 + sigma0(w7));
    Round(b, c, d, &e, f, g, h, &a, 0x76f988da831153b5ull, w7 += sigma1(w5) + w0 + sigma0(w8));
    Round(a, b, c, &d, e, f, g, &h, 0x983e5152ee66dfabull, w8 += sigma1(w6) + w1 + sigma0(w9));
    Round(h, a, b, &c, d, e, f, &g, 0xa831c66d2db43210ull, w9 += sigma1(w7) + w2 + sigma0(w10));
    Round(g, h, a, &b, c, d, e, &f, 0xb00327c898fb213full, w10 += sigma1(w8) + w3 + sigma0(w11));
    Round(f, g, h, &a, b, c, d, &e, 0xbf597fc7beef0ee4ull, w11 += sigma1(w9) + w4 + sigma0(w12));
    Round(e, f, g, &h, a, b, c, &d, 0xc6e00bf33da88fc2ull, w12 += sigma1(w10) + w5 + sigma0(w13));
    Round(d, e, f, &g, h, a, b, &c, 0xd5a79147930aa725ull, w13 += sigma1(w11) + w6 + sigma0(w14));
    Round(c, d, e, &f, g, h, a, &b, 0x06ca6351e003826full, w14 += sigma1(w12) + w7 + sigma0(w15));
    Round(b, c, d, &e, f, g, h, &a, 0x142929670a0e6e70ull, w15 += sigma1(w13) + w8 + sigma0(w0));

    Round(a, b, c, &d, e, f, g, &h, 0x27b70a8546d22ffcull, w0 += sigma1(w14) + w9 + sigma0(w1));
    Round(h, a, b, &c, d, e, f, &g, 0x2e1b21385c26c926ull, w1 += sigma1(w15) + w10 + sigma0(w2));
    Round(g, h, a, &b, c, d, e, &f, 0x4d2c6dfc5ac42aedull, w2 += sigma1(w0) + w11 + sigma0(w3));
    Round(f, g, h, &a, b, c, d, &e, 0x53380d139d95b3dfull, w3 += sigma1(w1) + w12 + sigma0(w4));
    Round(e, f, g, &h, a, b, c, &d, 0x650a73548baf63deull, w4 += sigma1(w2) + w13 + sigma0(w5));
    Round(d, e, f, &g, h, a, b, &c, 0x766a0abb3c77b2a8ull, w5 += sigma1(w3) + w14 + sigma0(w6));
    Round(c, d, e, &f, g, h, a, &b, 0x81c2c92e47edaee6ull, w6 += sigma1(w4) + w15 + sigma0(w7));
    Round(b, c, d, &e, f, g, h, &a, 0x92722c851482353bull, w7 += sigma1(w5) + w0 + sigma0(w8));
    Round(a, b, c, &d, e, f, g, &h, 0xa2bfe8a14cf10364ull, w8 += sigma1(w6) + w1 + sigma0(w9));
    Round(h, a, b, &c, d, e, f, &g, 0xa81a664bbc423001ull, w9 += sigma1(w7) + w2 + sigma0(w10));
    Round(g, h, a, &b, c, d, e, &f, 0xc24b8b70d0f89791ull, w10 += sigma1(w8) + w3 + sigma0(w11));
    Round(f, g, h, &a, b, c, d, &e, 0xc76c51a30654be30ull, w11 += sigma1(w9) + w4 + sigma0(w12));
    Round(e, f, g, &h, a, b, c, &d, 0xd192e819d6ef5218ull, w12 += sigma1(w10) + w5 + sigma0(w13));
    Round(d, e, f, &g, h, a, b, &c, 0xd69906245565a910ull, w13 += sigma1(w11) + w6 + sigma0(w14));
    Round(c, d, e, &f, g, h, a, &b, 0xf40e35855771202aull, w14 += sigma1(w12) + w7 + sigma0(w15));
    Round(b, c, d, &e, f, g, h, &a, 0x106aa07032bbd1b8ull, w15 += sigma1(w13) + w8 + sigma0(w0));

    Round(a, b, c, &d, e, f, g, &h, 0x19a4c116b8d2d0c8ull, w0 += sigma1(w14) + w9 + sigma0(w1));
    Round(h, a, b, &c, d, e, f, &g, 0x1e376c085141ab53ull, w1 += sigma1(w15) + w10 + sigma0(w2));
    Round(g, h, a, &b, c, d, e, &f, 0x2748774cdf8eeb99ull, w2 += sigma1(w0) + w11 + sigma0(w3));
    Round(f, g, h, &a, b, c, d, &e, 0x34b0bcb5e19b48a8ull, w3 += sigma1(w1) + w12 + sigma0(w4));
    Round(e, f, g, &h, a, b, c, &d, 0x391c0cb3c5c95a63ull, w4 += sigma1(w2) + w13 + sigma0(w5));
    Round(d, e, f, &g, h, a, b, &c, 0x4ed8aa4ae3418acbull, w5 += sigma1(w3) + w14 + sigma0(w6));
    Round(c, d, e, &f, g, h, a, &b, 0x5b9cca4f7763e373ull, w6 += sigma1(w4) + w15 + sigma0(w7));
    Round(b, c, d, &e, f, g, h, &a, 0x682e6ff3d6b2b8a3ull, w7 += sigma1(w5) + w0 + sigma0(w8));
    Round(a, b, c, &d, e, f, g, &h, 0x748f82ee5defb2fcull, w8 += sigma1(w6) + w1 + sigma0(w9));
    Round(h, a, b, &c, d, e, f, &g, 0x78a5636f43172f60ull, w9 += sigma1(w7) + w2 + sigma0(w10));
    Round(g, h, a, &b, c, d, e, &f, 0x84c87814a1f0ab72ull, w10 += sigma1(w8) + w3 + sigma0(w11));
    Round(f, g, h, &a, b, c, d, &e, 0x8cc702081a6439ecull, w11 += sigma1(w9) + w4 + sigma0(w12));
    Round(e, f, g, &h, a, b, c, &d, 0x90befffa23631e28ull, w12 += sigma1(w10) + w5 + sigma0(w13));
    Round(d, e, f, &g, h, a, b, &c, 0xa4506cebde82bde9ull, w13 += sigma1(w11) + w6 + sigma0(w14));
    Round(c, d, e, &f, g, h, a, &b, 0xbef9a3f7b2c67915ull, w14 += sigma1(w12) + w7 + sigma0(w15));
    Round(b, c, d, &e, f, g, h, &a, 0xc67178f2e372532bull, w15 += sigma1(w13) + w8 + sigma0(w0));

    Round(a, b, c, &d, e, f, g, &h, 0xca273eceea26619cull, w0 += sigma1(w14) + w9 + sigma0(w1));
    Round(h, a, b, &c, d, e, f, &g, 0xd186b8c721c0c207ull, w1 += sigma1(w15) + w10 + sigma0(w2));
    Round(g, h, a, &b, c, d, e, &f, 0xeada7dd6cde0eb1eull, w2 += sigma1(w0) + w11 + sigma0(w3));
    Round(f, g, h, &a, b, c, d, &e, 0xf57d4f7fee6ed178ull, w3 += sigma1(w1) + w12 + sigma0(w4));
    Round(e, f, g, &h, a, b, c, &d, 0x06f067aa72176fbaull, w4 += sigma1(w2) + w13 + sigma0(w5));
    Round(d, e, f, &g, h, a, b, &c, 0x0a637dc5a2c898a6ull, w5 += sigma1(w3) + w14 + sigma0(w6));
    Round(c, d, e, &f, g, h, a, &b, 0x113f9804bef90daeull, w6 += sigma1(w4) + w15 + sigma0(w7));
    Round(b, c, d, &e, f, g, h, &a, 0x1b710b35131c471bull, w7 += sigma1(w5) + w0 + sigma0(w8));
    Round(a, b, c, &d, e, f, g, &h, 0x28db77f523047d84ull, w8 += sigma1(w6) + w1 + sigma0(w9));
    Round(h, a, b, &c, d, e, f, &g, 0x32caab7b40c72493ull, w9 += sigma1(w7) + w2 + sigma0(w10));
    Round(g, h, a, &b, c, d, e, &f, 0x3c9ebe0a15c9bebcull, w10 += sigma1(w8) + w3 + sigma0(w11));
    Round(f, g, h, &a, b, c, d, &e, 0x431d67c49c100d4cull, w11 += sigma1(w9) + w4 + sigma0(w12));
    Round(e, f, g, &h, a, b, c, &d, 0x4cc5d4becb3e42b6ull, w12 += sigma1(w10) + w5 + sigma0(w13));
    Round(d, e, f, &g, h, a, b, &c, 0x597f299cfc657e2aull, w13 += sigma1(w11) + w6 + sigma0(w14));
    Round(c, d, e, &f, g, h, a, &b, 0x5fcb6fab3ad6faecull, w14 + sigma1(w12) + w7 + sigma0(w15));
    Round(b, c, d, &e, f, g, h, &a, 0x6c44198c4a475817ull, w15 + sigma1(w13) + w8 + sigma0(w0));

    s[0] += a;
    s[1] += b;
    s[2] += c;
    s[3] += d;
    s[4] += e;
    s[5] += f;
    s[6] += g;
    s[7] += h;
}


void sha512_init(sha512_ctx_t * sha)
{
	memset(sha, 0, sizeof(sha512_ctx_t));
	Initialize(sha->s);
}
void sha512_update(sha512_ctx_t * sha, const unsigned char * data, size_t len)
{
	const unsigned char * end = (const unsigned char *)data + len;
    size_t bufsize = sha->bytes % SHA512_BLOCK_SIZE;
    if (bufsize && bufsize + len >= SHA512_BLOCK_SIZE) {
        // Fill the buffer, and process it.
        memcpy(sha->buf + bufsize, data, SHA512_BLOCK_SIZE - bufsize);
        sha->bytes += SHA512_BLOCK_SIZE - bufsize;
        data += SHA512_BLOCK_SIZE - bufsize;
        Transform(sha->s, sha->buf);
        bufsize = 0;
    }
    while (end >= (data + 128)) {
        // Process full chunks directly from the source.
        Transform(sha->s, data);
        sha->bytes += SHA512_BLOCK_SIZE;
        data += SHA512_BLOCK_SIZE;
    }
    if (end > (const unsigned char *)data) {
        // Fill the buffer with what remains.
        memcpy(sha->buf + bufsize, data, end - (const unsigned char *)data);
        sha->bytes += end - (const unsigned char *)data;
    }

}
void sha512_final(sha512_ctx_t * sha, unsigned char hash[static 64])
{
	static const unsigned char pad[SHA512_BLOCK_SIZE] = {0x80};
	unsigned char sizedesc[16] = {0x00};
	uint64_t * s = sha->s;
	WriteBE64(sizedesc + 8, sha->bytes << 3);
	sha512_update(sha, pad, 1 + ((239 - (sha->bytes % SHA512_BLOCK_SIZE)) % SHA512_BLOCK_SIZE));
	sha512_update(sha, sizedesc, 16);
	WriteBE64(hash, s[0]);
	WriteBE64(hash + 8, s[1]);
	WriteBE64(hash + 16, s[2]);
	WriteBE64(hash + 24, s[3]);
	WriteBE64(hash + 32, s[4]);
	WriteBE64(hash + 40, s[5]);
	WriteBE64(hash + 48, s[6]);
	WriteBE64(hash + 56, s[7]);
}

#endif


#undef SHA512_BLOCK_SIZE

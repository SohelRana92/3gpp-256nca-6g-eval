-- r4_pkg.vhd - shared types, tables and functions for the R4 FPGA designs.
--
-- Block layout: byte k of a 16-byte block sits in bits 8k+7 downto 8k, so the
-- vector read as an unsigned number is the little-endian value of the block.
-- This is the POLYVAL bit order (bit i = coefficient of x^i) and also the AES
-- state order (state byte k = input byte k).
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package r4_pkg is
    subtype blk_t  is std_logic_vector(127 downto 0);
    subtype byte_t is std_logic_vector(7 downto 0);
    subtype word_t is std_logic_vector(31 downto 0);
    type byte_table is array (0 to 255) of byte_t;

    constant AES_SBOX : byte_table := (
        x"63", x"7c", x"77", x"7b", x"f2", x"6b", x"6f", x"c5", x"30", x"01", x"67", x"2b", x"fe", x"d7", x"ab", x"76",
        x"ca", x"82", x"c9", x"7d", x"fa", x"59", x"47", x"f0", x"ad", x"d4", x"a2", x"af", x"9c", x"a4", x"72", x"c0",
        x"b7", x"fd", x"93", x"26", x"36", x"3f", x"f7", x"cc", x"34", x"a5", x"e5", x"f1", x"71", x"d8", x"31", x"15",
        x"04", x"c7", x"23", x"c3", x"18", x"96", x"05", x"9a", x"07", x"12", x"80", x"e2", x"eb", x"27", x"b2", x"75",
        x"09", x"83", x"2c", x"1a", x"1b", x"6e", x"5a", x"a0", x"52", x"3b", x"d6", x"b3", x"29", x"e3", x"2f", x"84",
        x"53", x"d1", x"00", x"ed", x"20", x"fc", x"b1", x"5b", x"6a", x"cb", x"be", x"39", x"4a", x"4c", x"58", x"cf",
        x"d0", x"ef", x"aa", x"fb", x"43", x"4d", x"33", x"85", x"45", x"f9", x"02", x"7f", x"50", x"3c", x"9f", x"a8",
        x"51", x"a3", x"40", x"8f", x"92", x"9d", x"38", x"f5", x"bc", x"b6", x"da", x"21", x"10", x"ff", x"f3", x"d2",
        x"cd", x"0c", x"13", x"ec", x"5f", x"97", x"44", x"17", x"c4", x"a7", x"7e", x"3d", x"64", x"5d", x"19", x"73",
        x"60", x"81", x"4f", x"dc", x"22", x"2a", x"90", x"88", x"46", x"ee", x"b8", x"14", x"de", x"5e", x"0b", x"db",
        x"e0", x"32", x"3a", x"0a", x"49", x"06", x"24", x"5c", x"c2", x"d3", x"ac", x"62", x"91", x"95", x"e4", x"79",
        x"e7", x"c8", x"37", x"6d", x"8d", x"d5", x"4e", x"a9", x"6c", x"56", x"f4", x"ea", x"65", x"7a", x"ae", x"08",
        x"ba", x"78", x"25", x"2e", x"1c", x"a6", x"b4", x"c6", x"e8", x"dd", x"74", x"1f", x"4b", x"bd", x"8b", x"8a",
        x"70", x"3e", x"b5", x"66", x"48", x"03", x"f6", x"0e", x"61", x"35", x"57", x"b9", x"86", x"c1", x"1d", x"9e",
        x"e1", x"f8", x"98", x"11", x"69", x"d9", x"8e", x"94", x"9b", x"1e", x"87", x"e9", x"ce", x"55", x"28", x"df",
        x"8c", x"a1", x"89", x"0d", x"bf", x"e6", x"42", x"68", x"41", x"99", x"2d", x"0f", x"b0", x"54", x"bb", x"16");
    constant ZUC_S0 : byte_table := (
        x"3e", x"72", x"5b", x"47", x"ca", x"e0", x"00", x"33", x"04", x"d1", x"54", x"98", x"09", x"b9", x"6d", x"cb",
        x"7b", x"1b", x"f9", x"32", x"af", x"9d", x"6a", x"a5", x"b8", x"2d", x"fc", x"1d", x"08", x"53", x"03", x"90",
        x"4d", x"4e", x"84", x"99", x"e4", x"ce", x"d9", x"91", x"dd", x"b6", x"85", x"48", x"8b", x"29", x"6e", x"ac",
        x"cd", x"c1", x"f8", x"1e", x"73", x"43", x"69", x"c6", x"b5", x"bd", x"fd", x"39", x"63", x"20", x"d4", x"38",
        x"76", x"7d", x"b2", x"a7", x"cf", x"ed", x"57", x"c5", x"f3", x"2c", x"bb", x"14", x"21", x"06", x"55", x"9b",
        x"e3", x"ef", x"5e", x"31", x"4f", x"7f", x"5a", x"a4", x"0d", x"82", x"51", x"49", x"5f", x"ba", x"58", x"1c",
        x"4a", x"16", x"d5", x"17", x"a8", x"92", x"24", x"1f", x"8c", x"ff", x"d8", x"ae", x"2e", x"01", x"d3", x"ad",
        x"3b", x"4b", x"da", x"46", x"eb", x"c9", x"de", x"9a", x"8f", x"87", x"d7", x"3a", x"80", x"6f", x"2f", x"c8",
        x"b1", x"b4", x"37", x"f7", x"0a", x"22", x"13", x"28", x"7c", x"cc", x"3c", x"89", x"c7", x"c3", x"96", x"56",
        x"07", x"bf", x"7e", x"f0", x"0b", x"2b", x"97", x"52", x"35", x"41", x"79", x"61", x"a6", x"4c", x"10", x"fe",
        x"bc", x"26", x"95", x"88", x"8a", x"b0", x"a3", x"fb", x"c0", x"18", x"94", x"f2", x"e1", x"e5", x"e9", x"5d",
        x"d0", x"dc", x"11", x"66", x"64", x"5c", x"ec", x"59", x"42", x"75", x"12", x"f5", x"74", x"9c", x"aa", x"23",
        x"0e", x"86", x"ab", x"be", x"2a", x"02", x"e7", x"67", x"e6", x"44", x"a2", x"6c", x"c2", x"93", x"9f", x"f1",
        x"f6", x"fa", x"36", x"d2", x"50", x"68", x"9e", x"62", x"71", x"15", x"3d", x"d6", x"40", x"c4", x"e2", x"0f",
        x"8e", x"83", x"77", x"6b", x"25", x"05", x"3f", x"0c", x"30", x"ea", x"70", x"b7", x"a1", x"e8", x"a9", x"65",
        x"8d", x"27", x"1a", x"db", x"81", x"b3", x"a0", x"f4", x"45", x"7a", x"19", x"df", x"ee", x"78", x"34", x"60");
    constant ZUC_S1 : byte_table := (
        x"55", x"c2", x"63", x"71", x"3b", x"c8", x"47", x"86", x"9f", x"3c", x"da", x"5b", x"29", x"aa", x"fd", x"77",
        x"8c", x"c5", x"94", x"0c", x"a6", x"1a", x"13", x"00", x"e3", x"a8", x"16", x"72", x"40", x"f9", x"f8", x"42",
        x"44", x"26", x"68", x"96", x"81", x"d9", x"45", x"3e", x"10", x"76", x"c6", x"a7", x"8b", x"39", x"43", x"e1",
        x"3a", x"b5", x"56", x"2a", x"c0", x"6d", x"b3", x"05", x"22", x"66", x"bf", x"dc", x"0b", x"fa", x"62", x"48",
        x"dd", x"20", x"11", x"06", x"36", x"c9", x"c1", x"cf", x"f6", x"27", x"52", x"bb", x"69", x"f5", x"d4", x"87",
        x"7f", x"84", x"4c", x"d2", x"9c", x"57", x"a4", x"bc", x"4f", x"9a", x"df", x"fe", x"d6", x"8d", x"7a", x"eb",
        x"2b", x"53", x"d8", x"5c", x"a1", x"14", x"17", x"fb", x"23", x"d5", x"7d", x"30", x"67", x"73", x"08", x"09",
        x"ee", x"b7", x"70", x"3f", x"61", x"b2", x"19", x"8e", x"4e", x"e5", x"4b", x"93", x"8f", x"5d", x"db", x"a9",
        x"ad", x"f1", x"ae", x"2e", x"cb", x"0d", x"fc", x"f4", x"2d", x"46", x"6e", x"1d", x"97", x"e8", x"d1", x"e9",
        x"4d", x"37", x"a5", x"75", x"5e", x"83", x"9e", x"ab", x"82", x"9d", x"b9", x"1c", x"e0", x"cd", x"49", x"89",
        x"01", x"b6", x"bd", x"58", x"24", x"a2", x"5f", x"38", x"78", x"99", x"15", x"90", x"50", x"b8", x"95", x"e4",
        x"d0", x"91", x"c7", x"ce", x"ed", x"0f", x"b4", x"6f", x"a0", x"cc", x"f0", x"02", x"4a", x"79", x"c3", x"de",
        x"a3", x"ef", x"ea", x"51", x"e6", x"6b", x"18", x"ec", x"1b", x"2c", x"80", x"f7", x"74", x"e7", x"ff", x"21",
        x"5a", x"6a", x"54", x"1e", x"41", x"31", x"92", x"35", x"c4", x"33", x"07", x"0a", x"ba", x"7e", x"0e", x"34",
        x"88", x"b1", x"98", x"7c", x"f3", x"3d", x"60", x"6c", x"7b", x"ca", x"d3", x"1f", x"32", x"65", x"04", x"28",
        x"64", x"be", x"85", x"9b", x"2f", x"59", x"8a", x"d7", x"b0", x"25", x"ac", x"af", x"12", x"03", x"e2", x"f2");

    function get_byte(b : std_logic_vector; k : natural) return byte_t;
    function aes_sbox_f(x : byte_t) return byte_t;
    function xtime(x : byte_t) return byte_t;
    function sub_bytes(s : blk_t) return blk_t;
    function shift_rows(s : blk_t) return blk_t;
    function mix_columns(s : blk_t) return blk_t;
    function aes_round_zero_key(s : blk_t) return blk_t;            -- SubBytes, ShiftRows, MixColumns
    function sub_word(w : word_t) return word_t;                    -- byte 0 in bits 7..0
    function rot_word(w : word_t) return word_t;                    -- (a0,a1,a2,a3) -> (a1,a2,a3,a0)
    function bit_mask(nbits : natural) return blk_t;                -- the first nbits of a block
end package;

package body r4_pkg is
    function get_byte(b : std_logic_vector; k : natural) return byte_t is
        variable v : std_logic_vector(b'length - 1 downto 0) := b;
    begin
        return v(8 * k + 7 downto 8 * k);
    end function;

    function aes_sbox_f(x : byte_t) return byte_t is
    begin
        return AES_SBOX(to_integer(unsigned(x)));
    end function;

    function xtime(x : byte_t) return byte_t is
        variable r : byte_t := x(6 downto 0) & '0';
    begin
        if x(7) = '1' then r := r xor x"1b"; end if;
        return r;
    end function;

    function sub_bytes(s : blk_t) return blk_t is
        variable r : blk_t;
    begin
        for k in 0 to 15 loop r(8 * k + 7 downto 8 * k) := aes_sbox_f(s(8 * k + 7 downto 8 * k)); end loop;
        return r;
    end function;

    -- new state byte (r + 4c) = old byte (r + 4((c + r) mod 4))
    function shift_rows(s : blk_t) return blk_t is
        variable r : blk_t;
    begin
        for c in 0 to 3 loop
            for rr in 0 to 3 loop
                r(8 * (rr + 4 * c) + 7 downto 8 * (rr + 4 * c)) := s(8 * (rr + 4 * ((c + rr) mod 4)) + 7 downto 8 * (rr + 4 * ((c + rr) mod 4)));
            end loop;
        end loop;
        return r;
    end function;

    function mix_columns(s : blk_t) return blk_t is
        variable r : blk_t;
        variable a0, a1, a2, a3 : byte_t;
    begin
        for c in 0 to 3 loop
            a0 := s(32 * c + 7 downto 32 * c);      a1 := s(32 * c + 15 downto 32 * c + 8);
            a2 := s(32 * c + 23 downto 32 * c + 16); a3 := s(32 * c + 31 downto 32 * c + 24);
            r(32 * c + 7 downto 32 * c)       := xtime(a0) xor (xtime(a1) xor a1) xor a2 xor a3;
            r(32 * c + 15 downto 32 * c + 8)  := a0 xor xtime(a1) xor (xtime(a2) xor a2) xor a3;
            r(32 * c + 23 downto 32 * c + 16) := a0 xor a1 xor xtime(a2) xor (xtime(a3) xor a3);
            r(32 * c + 31 downto 32 * c + 24) := (xtime(a0) xor a0) xor a1 xor a2 xor xtime(a3);
        end loop;
        return r;
    end function;

    function aes_round_zero_key(s : blk_t) return blk_t is
    begin
        return mix_columns(shift_rows(sub_bytes(s)));
    end function;

    function sub_word(w : word_t) return word_t is
    begin
        return aes_sbox_f(w(31 downto 24)) & aes_sbox_f(w(23 downto 16)) & aes_sbox_f(w(15 downto 8)) & aes_sbox_f(w(7 downto 0));
    end function;

    function rot_word(w : word_t) return word_t is
    begin
        return w(7 downto 0) & w(31 downto 8);
    end function;

    -- byte k fully kept for k < nbits/8; the next byte keeps its (nbits mod 8) leading (most significant) bits
    function bit_mask(nbits : natural) return blk_t is
        variable m : blk_t := (others => '0');
        variable nb : natural := nbits / 8;
        variable rb : natural := nbits mod 8;
    begin
        for k in 0 to 15 loop
            if k < nb then
                m(8 * k + 7 downto 8 * k) := x"ff";
            elsif k = nb and rb /= 0 then
                for j in 0 to 7 loop
                    if j >= 8 - rb then m(8 * k + j) := '1'; end if;
                end loop;
            end if;
        end loop;
        return m;
    end function;
end package body;

-- ksg_aes.vhd - AES-256 keystream generator of the 256-NxA5 set (TS 35.243, clause 6.3).
--
-- AES-256 in counter mode. Counter block = IV bytes 0..11 || 32-bit big-endian
-- counter. With MAC enabled, H, Q and P are counters 0, 1, 2 with the AI bit
-- (bit 0 of IV byte 0) set; the keystream then restarts at counter 0 with AI = 0.
-- Iterative datapath with RPC rounds per clock (1 or 2) and the key schedule
-- computed on the fly (4 S-boxes per round), so no round keys are stored.
-- Throughput: one block per 14 / RPC clocks; the next block starts in the clock
-- after the last round. A one-block output buffer decouples the consumer.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.r4_pkg.all;

entity ksg_aes is
    generic (RPC : positive := 1);                   -- AES rounds per clock: 1 or 2
    port (
        clk, rst : in  std_logic;
        init     : in  std_logic;
        key      : in  std_logic_vector(255 downto 0);
        iv       : in  blk_t;
        mac_en   : in  std_logic;
        take     : in  std_logic;
        valid    : out std_logic;
        ks       : out blk_t
    );
end entity;

architecture rtl of ksg_aes is
    type rcon_t is array (1 to 7) of byte_t;
    constant RCON : rcon_t := (x"01", x"02", x"04", x"08", x"10", x"20", x"40");

    -- round key r (r >= 2) from round keys r-2 and r-1 (FIPS 197, 5.2, Nk = 8)
    function expand(prev, cur : blk_t; r : natural) return blk_t is
        variable t : word_t := cur(127 downto 96);
        variable w : blk_t;
    begin
        if r mod 2 = 0 then
            t := sub_word(rot_word(t));
            t(7 downto 0) := t(7 downto 0) xor RCON(r / 2);
        else
            t := sub_word(t);
        end if;
        w(31 downto 0)   := prev(31 downto 0) xor t;
        w(63 downto 32)  := prev(63 downto 32) xor w(31 downto 0);
        w(95 downto 64)  := prev(95 downto 64) xor w(63 downto 32);
        w(127 downto 96) := prev(127 downto 96) xor w(95 downto 64);
        return w;
    end function;

    function full_round(s, rk : blk_t) return blk_t is
    begin
        return mix_columns(shift_rows(sub_bytes(s))) xor rk;
    end function;

    function last_round(s, rk : blk_t) return blk_t is
    begin
        return shift_rows(sub_bytes(s)) xor rk;
    end function;

    signal kreg              : std_logic_vector(255 downto 0);
    signal ivr               : blk_t;
    signal ai                : std_logic;
    signal ctr               : unsigned(31 downto 0);
    signal nh                : natural range 0 to 3;
    signal s, prev, cur      : blk_t;
    signal r                 : natural range 1 to 15;
    signal running, holding  : std_logic := '0';
    signal res               : blk_t;
    signal outr              : blk_t;
    signal out_full          : std_logic := '0';

    function counter_block(ivv : blk_t; aib : std_logic; c : unsigned(31 downto 0)) return blk_t is
        variable b : blk_t := ivv;
    begin
        b(0) := ivv(0) or aib;
        b(103 downto 96)  := std_logic_vector(c(31 downto 24));
        b(111 downto 104) := std_logic_vector(c(23 downto 16));
        b(119 downto 112) := std_logic_vector(c(15 downto 8));
        b(127 downto 120) := std_logic_vector(c(7 downto 0));
        return b;
    end function;
begin
    valid <= out_full;
    ks    <= outr;

    process (clk)
        variable s1, s2, k1, k2, blk : blk_t;
        variable fin                 : boolean;
        variable can_put             : boolean;
        variable n_ai                : std_logic;
        variable n_ctr               : unsigned(31 downto 0);
        variable n_nh                : natural range 0 to 3;
    begin
        if rising_edge(clk) then
            if rst = '1' then
                running <= '0'; holding <= '0'; out_full <= '0';
            else
                can_put := out_full = '0' or take = '1';
                if take = '1' then out_full <= '0'; end if;

                fin := false;
                if running = '1' then
                    if RPC = 1 then
                        if r = 14 then
                            blk := last_round(s, cur); fin := true;
                        else
                            s <= full_round(s, cur);
                            prev <= cur; cur <= expand(prev, cur, r + 1);
                            r <= r + 1;
                        end if;
                    else
                        s1 := full_round(s, cur);               -- round r (odd, <= 13)
                        k1 := expand(prev, cur, r + 1);
                        if r + 1 = 14 then
                            blk := last_round(s1, k1); fin := true;
                        else
                            s <= full_round(s1, k1);             -- round r + 1
                            k2 := expand(cur, k1, r + 2);
                            prev <= k1; cur <= k2;
                            r <= r + 2;
                        end if;
                    end if;
                elsif holding = '1' then
                    blk := res; fin := true;
                end if;

                if fin then
                    if can_put then
                        outr <= blk; out_full <= '1'; holding <= '0';
                        -- next counter / AI
                        n_ai := ai; n_ctr := ctr + 1; n_nh := nh;
                        if ai = '1' then
                            if nh = 2 then n_ai := '0'; n_ctr := (others => '0'); n_nh := 0;
                            else n_nh := nh + 1; end if;
                        end if;
                        ai <= n_ai; ctr <= n_ctr; nh <= n_nh;
                        s <= counter_block(ivr, n_ai, n_ctr) xor kreg(127 downto 0);
                        prev <= kreg(127 downto 0); cur <= kreg(255 downto 128); r <= 1;
                        running <= '1';
                    else
                        res <= blk; holding <= '1'; running <= '0';
                    end if;
                end if;

                if init = '1' then
                    kreg <= key; ivr <= iv; ai <= mac_en; ctr <= (others => '0'); nh <= 0;
                    s <= counter_block(iv, mac_en, (others => '0')) xor key(127 downto 0);
                    prev <= key(127 downto 0); cur <= key(255 downto 128); r <= 1;
                    running <= '1'; holding <= '0'; out_full <= '0';
                end if;
            end if;
        end if;
    end process;
end architecture;

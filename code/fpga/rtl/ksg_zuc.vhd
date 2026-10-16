-- ksg_zuc.vhd - ZUC-256 keystream generator of the 256-NxA6 set (TS 35.246, clause 6).
--
-- One LFSR step per cycle: one 32-bit word per cycle in work mode, so one
-- 128-bit block per 4 cycles. Initialisation: 48 steps in initialisation mode,
-- then one step whose F output is discarded (P = 48, clause 6.3.1). Each
-- initialisation step takes two cycles (F, then the LFSR with W >> 1 from a
-- register), which keeps the adder of F out of the feedback path.
-- The LFSR feedback, a sum of up to seven terms mod 2^31 - 1, uses carry-save
-- steps with end-around carries and a single carry-propagate addition.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.r4_pkg.all;

entity ksg_zuc is
    generic (ARCH : positive := 1);                  -- one architecture only
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

architecture rtl of ksg_zuc is
    subtype c31 is unsigned(30 downto 0);
    type lfsr_t is array (0 to 15) of c31;
    type d_t is array (0 to 15) of std_logic_vector(6 downto 0);
    constant DC : d_t := ("1100100", "1000011", "1111011", "0101010", "0010001", "0000101", "1010001", "1000010",
                          "0011010", "0110001", "0011000", "1100110", "0010100", "0101110", "0000001", "1011100");

    -- x + y mod 2^31 - 1 with end-around carry: both x + y and x + y + 1 are formed
    -- in parallel and the carry out of bit 30 selects one (one carry chain deep)
    function add31(x, y : c31) return c31 is
        variable s0 : unsigned(31 downto 0) := ('0' & x) + ('0' & y);
        variable s1 : unsigned(31 downto 0) := ('0' & x) + ('0' & y) + 1;
    begin
        if s0(31) = '1' then return s1(30 downto 0); else return s0(30 downto 0); end if;
    end function;

    -- carry-save step mod 2^31 - 1: three operands to two with the same sum
    -- (the carry out of bit 30 is worth 2^31 = 1, so the carries rotate by one)
    procedure csa(a, b, c : c31; variable s, t : out c31) is
        variable cy : c31;
    begin
        s  := a xor b xor c;
        cy := (a and b) or (a and c) or (b and c);
        t  := cy(29 downto 0) & cy(30);
    end procedure;

    function rot31(x : c31; k : natural) return c31 is
    begin
        return x(30 - k downto 0) & x(30 downto 31 - k);
    end function;

    function cell(k : byte_t; d : std_logic_vector(6 downto 0); b2, b3 : byte_t) return c31 is
        variable v : std_logic_vector(30 downto 0);
    begin
        v(30 downto 23) := k; v(22 downto 16) := d; v(15 downto 8) := b2; v(7 downto 0) := b3;
        return unsigned(v);
    end function;

    function sbox32(x : word_t) return word_t is
    begin
        return ZUC_S0(to_integer(unsigned(x(31 downto 24)))) & ZUC_S1(to_integer(unsigned(x(23 downto 16)))) &
               ZUC_S0(to_integer(unsigned(x(15 downto 8))))  & ZUC_S1(to_integer(unsigned(x(7 downto 0))));
    end function;

    function rotl(x : word_t; k : natural) return word_t is
    begin
        return x(31 - k downto 0) & x(31 downto 32 - k);
    end function;

    function l1(x : word_t) return word_t is
    begin return x xor rotl(x, 2) xor rotl(x, 10) xor rotl(x, 18) xor rotl(x, 24); end function;
    function l2(x : word_t) return word_t is
    begin return x xor rotl(x, 8) xor rotl(x, 14) xor rotl(x, 22) xor rotl(x, 30); end function;

    signal s            : lfsr_t;
    signal r1, r2       : word_t;
    signal stepn        : natural range 0 to 49 := 0;    -- 0..47 init, 48 final, 49 work
    signal wi           : natural range 0 to 3 := 0;
    signal acc          : std_logic_vector(95 downto 0);
    signal running      : std_logic := '0';
    signal half         : std_logic := '0';       -- initialisation: F done, LFSR update next
    signal u_reg        : c31;                    -- W >> 1, registered
    signal outr         : blk_t;
    signal out_full     : std_logic := '0';
begin
    valid <= out_full;
    ks    <= outr;

    process (clk)
        variable x0, x1, x2, x3, w, w1, w2, z : word_t;
        variable v, u, p1, q1, p2, q2, p3, q3, p4, q4, p5, q5 : c31;
        variable can_put, do_step             : boolean;
        variable k, ivb                       : std_logic_vector(7 downto 0);
        variable blk                          : blk_t;
    begin
        if rising_edge(clk) then
            if rst = '1' then
                running <= '0'; out_full <= '0';
            else
                can_put := out_full = '0' or take = '1';
                if take = '1' then out_full <= '0'; end if;
                do_step := running = '1' and (stepn < 49 or wi < 3 or can_put);

                if do_step then
                    -- bit reorganisation
                    x0 := std_logic_vector(s(15)(30 downto 15)) & std_logic_vector(s(14)(15 downto 0));
                    x1 := std_logic_vector(s(11)(15 downto 0)) & std_logic_vector(s(9)(30 downto 15));
                    x2 := std_logic_vector(s(7)(15 downto 0)) & std_logic_vector(s(5)(30 downto 15));
                    x3 := std_logic_vector(s(2)(15 downto 0)) & std_logic_vector(s(0)(30 downto 15));
                    -- F
                    w  := std_logic_vector(unsigned(x0 xor r1) + unsigned(r2));
                    w1 := std_logic_vector(unsigned(r1) + unsigned(x1));
                    w2 := r2 xor x2;
                    -- LFSR feedback: six terms (and u = W >> 1, registered, in initialisation
                    -- mode) reduced to two by carry-save steps, then one end-around addition
                    if stepn < 48 then u := u_reg; else u := (others => '0'); end if;
                    csa(s(0), rot31(s(0), 8), rot31(s(4), 20), p1, q1);
                    csa(rot31(s(10), 21), rot31(s(13), 17), rot31(s(15), 15), p2, q2);
                    csa(p1, q1, p2, p3, q3);
                    csa(p3, q3, q2, p4, q4);
                    csa(p4, q4, u, p5, q5);
                    v := add31(p5, q5);
                    if p5 = 0 and q5 = 0 then v := (others => '1'); end if;

                    if stepn < 48 and half = '0' then
                        -- initialisation, first cycle of a step: F only (the LFSR waits for W)
                        r1 <= sbox32(l1(w1(15 downto 0) & w2(31 downto 16)));
                        r2 <= sbox32(l2(w2(15 downto 0) & w1(31 downto 16)));
                        u_reg <= unsigned(w(31 downto 1));
                        half <= '1';
                    else
                        if stepn >= 48 then                 -- work mode (and the final init clock): F and LFSR together
                            r1 <= sbox32(l1(w1(15 downto 0) & w2(31 downto 16)));
                            r2 <= sbox32(l2(w2(15 downto 0) & w1(31 downto 16)));
                        end if;
                        half <= '0';
                        for i in 0 to 14 loop s(i) <= s(i + 1); end loop;
                        s(15) <= v;
                        if stepn < 49 then
                            stepn <= stepn + 1;
                        else
                            z := w xor x3;
                            if wi = 3 then
                                blk := (z(7 downto 0) & z(15 downto 8) & z(23 downto 16) & z(31 downto 24)) & acc;
                                outr <= blk; out_full <= '1'; wi <= 0;
                            else
                                -- word i into bytes 4i..4i+3, most significant byte first
                                acc(32 * wi + 31 downto 32 * wi) <= z(7 downto 0) & z(15 downto 8) & z(23 downto 16) & z(31 downto 24);
                                wi <= wi + 1;
                            end if;
                        end if;
                    end if;
                end if;

                if init = '1' then
                    for i in 0 to 15 loop
                        k := key(8 * i + 7 downto 8 * i);
                        if i < 7 then
                            s(i) <= cell(k, DC(i), key(8 * (16 + i) + 7 downto 8 * (16 + i)), key(8 * (24 + i) + 7 downto 8 * (24 + i)));
                        elsif i < 15 then
                            s(i) <= cell(k, DC(i), iv(8 * (i - 7) + 7 downto 8 * (i - 7)), iv(8 * (i + 1) + 7 downto 8 * (i + 1)));
                        else
                            s(i) <= cell(k, DC(i), key(8 * 23 + 7 downto 8 * 23), key(8 * 31 + 7 downto 8 * 31));
                        end if;
                    end loop;
                    r1 <= (others => '0'); r2 <= (others => '0');
                    stepn <= 0; wi <= 0; running <= '1'; out_full <= '0'; half <= '0';
                end if;
            end if;
        end if;
    end process;
end architecture;

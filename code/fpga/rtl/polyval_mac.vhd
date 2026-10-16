-- polyval_mac.vhd - Mac5G accumulator of 256-AEAD1 (TS 35.243, clause 5.2.3).
--
-- absorb : A <- (A xor X) . H
-- final  : A <- ((A xor L) . Q) xor P
-- where a . b = a * b * x^-128 in GF(2^128) mod x^128 + x^127 + x^126 + x^121 + 1
-- (POLYVAL, RFC 8452). The multiplier is digit-serial: each clock handles D bits
-- of the operand y = A xor X, least significant first: acc <- (acc + y_digit * m) * x^-D,
-- with a Montgomery division by x^D. After 128/D clocks acc = m * y * x^-128 exactly.
-- A new operation can start in the last clock of the previous one (no bubble).
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.r4_pkg.all;

entity polyval_mac is
    generic (D : positive := 8);                     -- digit size: 1, 2, 4, 8, 16, 32, 64 or 128
    port (
        clk, rst   : in  std_logic;
        clear      : in  std_logic;                  -- A <- 0
        load_h     : in  std_logic;                  -- H, Q, P <- key_in (one per pulse, in this order)
        key_in     : in  blk_t;
        start      : in  std_logic;                  -- start absorb (final = '0') or final (final = '1')
        final      : in  std_logic;
        x_in       : in  blk_t;                      -- X for absorb, L for final
        ready      : out std_logic;                  -- start is accepted in this clock
        done       : out std_logic;                  -- the last operation is complete (A valid)
        a_out      : out blk_t
    );
end entity;

architecture rtl of polyval_mac is
    constant STEPS : natural := 128 / D;
    signal h, q, p, a, acc, y, m : blk_t := (others => '0');
    signal cnt     : natural range 0 to STEPS := 0;
    signal busy, is_final : std_logic := '0';
    signal hsel    : natural range 0 to 3 := 0;

    -- D interleaved steps at once: T = acc + sum_{j<D} y_j * mm * x^j (no reduction), then the
    -- Montgomery division by x^D: clear the D low bits of T with multiples of p and shift.
    -- Since p = 1 + x^121 + x^126 + x^127 + x^128, clearing bit j touches only bit j and bits
    -- j + 121 and up, so for D <= 121 every decision uses an original bit of T (no chain).
    procedure do_steps(variable va, vy : inout blk_t; mm : blk_t) is
        variable t : std_logic_vector(127 + D downto 0) := (others => '0');
    begin
        t(127 downto 0) := va;
        for j in 0 to D - 1 loop
            if vy(j) = '1' then t(127 + j downto j) := t(127 + j downto j) xor mm; end if;
        end loop;
        for j in 0 to D - 1 loop
            if t(j) = '1' then
                t(j) := '0';
                t(j + 121) := not t(j + 121); t(j + 126) := not t(j + 126);
                t(j + 127) := not t(j + 127); t(j + 128) := not t(j + 128);
            end if;
        end loop;
        va := t(127 + D downto D);
        if D < 128 then vy := (D - 1 downto 0 => '0') & vy(127 downto D); else vy := (others => '0'); end if;
    end procedure;

    signal acc_next, y_next, result : blk_t;
    signal last : std_logic;
begin
    process (acc, y, m, cnt, busy, is_final, p)
        variable va, vy : blk_t;
    begin
        va := acc; vy := y;
        do_steps(va, vy, m);
        acc_next <= va; y_next <= vy;
        if is_final = '1' then result <= va xor p; else result <= va; end if;
    end process;

    last  <= '1' when busy = '1' and cnt = STEPS - 1 else '0';
    ready <= (not busy) or last;
    done  <= not busy;
    a_out <= a;

    process (clk)
        variable anew : blk_t;
    begin
        if rising_edge(clk) then
            if rst = '1' then
                busy <= '0'; cnt <= 0; hsel <= 0; a <= (others => '0');
            else
                if load_h = '1' then
                    case hsel is
                        when 0 => h <= key_in;
                        when 1 => q <= key_in;
                        when others => p <= key_in;
                    end case;
                    if hsel < 2 then hsel <= hsel + 1; else hsel <= 0; end if;
                end if;
                anew := a;
                if last = '1' then anew := result; a <= result; busy <= '0'; end if;
                if busy = '1' and last = '0' then
                    acc <= acc_next; y <= y_next; cnt <= cnt + 1;
                end if;
                if clear = '1' then a <= (others => '0'); anew := (others => '0'); hsel <= 0; end if;
                if start = '1' and (busy = '0' or last = '1') then
                    y <= anew xor x_in;
                    acc <= (others => '0');
                    if final = '1' then m <= q; else m <= h; end if;
                    is_final <= final;
                    cnt <= 0;
                    busy <= '1';
                end if;
            end if;
        end if;
    end process;
end architecture;

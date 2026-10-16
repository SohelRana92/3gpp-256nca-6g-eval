-- ksg_snow.vhd - Snow 5G keystream generator of the 256-NxA4 set (TS 35.240, clause 6).
--
-- One Snow 5G clock gives one 128-bit keystream word. The clock updates all
-- eight new cells of each LFSR at once and the three FSM registers, which needs
-- two AES rounds (32 S-boxes).
--   ARCH = 2 : both AES rounds in one cycle  -> one block per cycle
--   ARCH = 1 : one AES round per cycle (16 S-boxes, shared) -> one block per 2 cycles
-- Initialisation: 16 clocks, then the keystream (H, Q, P first when MAC is used).
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.r4_pkg.all;

entity ksg_snow is
    generic (ARCH : positive := 2);
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

architecture rtl of ksg_snow is
    type sig_t is array (0 to 15) of natural;
    constant SIGMA : sig_t := (0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15);

    function add16(x, y : blk_t) return blk_t is
        variable r : blk_t;
    begin
        for i in 0 to 7 loop
            r(16 * i + 15 downto 16 * i) := std_logic_vector(unsigned(x(16 * i + 15 downto 16 * i)) + unsigned(y(16 * i + 15 downto 16 * i)));
        end loop;
        return r;
    end function;

    function sigma_f(t : blk_t) return blk_t is
        variable r : blk_t;
    begin
        for j in 0 to 15 loop r(8 * j + 7 downto 8 * j) := t(8 * SIGMA(j) + 7 downto 8 * SIGMA(j)); end loop;
        return r;
    end function;

    function mulx(v : std_logic_vector(15 downto 0); poly : std_logic_vector(15 downto 0)) return std_logic_vector is
        variable r : std_logic_vector(15 downto 0) := v(14 downto 0) & '0';
    begin
        if v(15) = '1' then r := r xor poly; end if;
        return r;
    end function;

    signal a, b           : std_logic_vector(255 downto 0);     -- cell i in bits 16i+15 .. 16i
    signal r1, r2, r3     : blk_t;
    signal kreg_lo, kreg_hi : blk_t;
    signal t_init         : natural range 0 to 16 := 0;          -- clocks done in initialisation
    signal running        : std_logic := '0';
    signal phase          : std_logic := '0';                    -- ARCH = 1: '0' = first half
    signal r3_hold        : blk_t;                               -- AES_R(R2) from the first half
    signal outr           : blk_t;
    signal out_full       : std_logic := '0';
    signal aes_in, aes_out : blk_t;
begin
    valid <= out_full;
    ks    <= outr;

    -- the shared AES round (ARCH = 1); ARCH = 2 instantiates two in the process below
    aes_in  <= r2 when phase = '0' else r1;
    aes_out <= aes_round_zero_key(aes_in);

    process (clk)
        variable z, tmp, n_r1, n_r2, n_r3 : blk_t;
        variable na, nb                   : blk_t;
        variable a_new, b_new             : std_logic_vector(255 downto 0);
        variable do_clock, in_init        : boolean;
        variable can_put                  : boolean;
    begin
        if rising_edge(clk) then
            if rst = '1' then
                running <= '0'; out_full <= '0'; phase <= '0';
            else
                can_put := out_full = '0' or take = '1';
                if take = '1' then out_full <= '0'; end if;
                in_init := t_init < 16;

                do_clock := false;
                if running = '1' then
                    if ARCH = 2 then
                        do_clock := in_init or can_put;
                        n_r3 := aes_round_zero_key(r2);
                        n_r2 := aes_round_zero_key(r1);
                    else
                        if phase = '0' then
                            if in_init or can_put then r3_hold <= aes_out; phase <= '1'; end if;
                        else
                            do_clock := true;      -- can_put held: nothing else takes the buffer
                            n_r3 := r3_hold;
                            n_r2 := aes_out;
                            phase <= '0';
                        end if;
                    end if;
                end if;

                if do_clock then
                    z   := add16(r1, b(255 downto 128)) xor r2;                    -- (R1 [+] T1) xor R2
                    tmp := add16(r2, r3 xor a(255 downto 128));                    -- R2 [+] (R3 xor T2)
                    n_r1 := sigma_f(tmp);
                    for j in 0 to 7 loop
                        na(16 * j + 15 downto 16 * j) := b(16 * j + 15 downto 16 * j) xor a(16 * (7 + j) + 15 downto 16 * (7 + j))
                                                         xor mulx(a(16 * j + 15 downto 16 * j), x"4a6d");
                        nb(16 * j + 15 downto 16 * j) := a(16 * j + 15 downto 16 * j) xor b(16 * (8 + j) + 15 downto 16 * (8 + j))
                                                         xor mulx(b(16 * j + 15 downto 16 * j), x"cc87");
                    end loop;
                    a_new := na & a(255 downto 128);
                    b_new := nb & b(255 downto 128);
                    if in_init then
                        a_new(255 downto 128) := a_new(255 downto 128) xor z;      -- (a15..a8) ^= z
                        if t_init = 14 then n_r1 := n_r1 xor kreg_lo; end if;      -- after clock 15
                        if t_init = 15 then n_r1 := n_r1 xor kreg_hi; end if;      -- after clock 16
                        t_init <= t_init + 1;
                    else
                        outr <= z; out_full <= '1';
                    end if;
                    a <= a_new; b <= b_new; r1 <= n_r1; r2 <= n_r2; r3 <= n_r3;
                end if;

                if init = '1' then
                    a <= key(127 downto 0) & iv;                    -- a8..a15 = k0..k7, a0..a7 = iv0..iv7
                    b <= key(255 downto 128) & (127 downto 0 => '0');
                    r1 <= (others => '0'); r2 <= (others => '0'); r3 <= (others => '0');
                    kreg_lo <= key(127 downto 0); kreg_hi <= key(255 downto 128);
                    t_init <= 0; running <= '1'; phase <= '0'; out_full <= '0';
                end if;
            end if;
        end if;
    end process;
end architecture;

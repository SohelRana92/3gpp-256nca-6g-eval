-- tb_polyval.vhd - checks polyval_mac against products computed by the C reference.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;
use work.r4_pkg.all;

entity tb_polyval is
    generic (D : positive := 8; VEC : string := "tb/vectors/polyval.txt");
end entity;

architecture sim of tb_polyval is
    signal clk : std_logic := '0';
    signal rst, clear, load_h, start, final, ready, done : std_logic := '0';
    signal key_in, x_in, a_out : blk_t := (others => '0');

    function hex2blk(s : string) return blk_t is      -- 32 hex chars, byte 0 first
        variable r : blk_t;
        variable v : natural;
        variable c : character;
    begin
        for i in 0 to 31 loop
            c := s(s'low + i);
            if c >= '0' and c <= '9' then v := character'pos(c) - 48; else v := character'pos(c) - 87; end if;
            -- char 2k is the high nibble of byte k
            if i mod 2 = 0 then r(8 * (i / 2) + 7 downto 8 * (i / 2) + 4) := std_logic_vector(to_unsigned(v, 4));
            else r(8 * (i / 2) + 3 downto 8 * (i / 2)) := std_logic_vector(to_unsigned(v, 4)); end if;
        end loop;
        return r;
    end function;
begin
    clk <= not clk after 5 ns;
    dut : entity work.polyval_mac generic map (D => D)
        port map (clk, rst, clear, load_h, key_in, start, final, x_in, ready, done, a_out);

    process
        file f : text open read_mode is VEC;
        variable l : line;
        variable sa, sb, sr : string(1 to 32);
        variable n, bad : natural := 0;
    begin
        rst <= '1'; wait until rising_edge(clk); rst <= '0';
        while not endfile(f) loop
            readline(f, l); read(l, sa); readline(f, l); read(l, sb); readline(f, l); read(l, sr);
            -- dot(a, b): H <- b, A <- 0, absorb a
            clear <= '1'; load_h <= '1'; key_in <= hex2blk(sb);
            wait until rising_edge(clk);
            clear <= '0'; load_h <= '0';
            start <= '1'; final <= '0'; x_in <= hex2blk(sa);
            wait until rising_edge(clk);
            start <= '0';
            wait until rising_edge(clk) and done = '1';
            if a_out /= hex2blk(sr) then bad := bad + 1; end if;
            n := n + 1;
        end loop;
        report "POLYVAL D=" & integer'image(D) & ": " & integer'image(n) & " products, " & integer'image(bad) & " mismatches"
            severity note;
        if bad /= 0 then report "FAIL" severity failure; end if;
        std.env.finish;
    end process;
end architecture;

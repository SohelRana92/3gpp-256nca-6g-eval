-- tb_hwtest.vhd - drives hwtest_top over its UART with one packet from a vector
-- file and checks the reply (output blocks and tag). Uses a fast UART (10 clocks per bit).
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;
use work.r4_pkg.all;

entity tb_hwtest is
    generic (SET : natural := 5; ARCH : positive := 1; D : positive := 8;
             VEC : string := "tb/vectors/vectors_nca5.txt"; NPKT : natural := 12);
end entity;

architecture sim of tb_hwtest is
    constant CPB : natural := 10;
    signal clk : std_logic := '0';
    signal txd, rxd : std_logic := '1';      -- txd: PC -> board
    signal led : std_logic_vector(3 downto 0);
    type bytes_t is array (0 to 40000) of std_logic_vector(7 downto 0);

    function hexb(s : string; i : natural) return std_logic_vector is
        variable v : natural := 0;
        variable c : character;
    begin
        for k in 0 to 1 loop
            c := s(s'low + 2 * i + k);
            if c >= '0' and c <= '9' then v := v * 16 + character'pos(c) - 48; else v := v * 16 + character'pos(c) - 87; end if;
        end loop;
        return std_logic_vector(to_unsigned(v, 8));
    end function;
begin
    clk <= not clk after 5 ns;
    dut : entity work.hwtest_top generic map (SET => SET, ARCH => ARCH, D => D, CLKS_PER_BIT => CPB)
        port map (clk, txd, rxd, led);

    process
        file f : text open read_mode is VEC;
        variable l : line;
        variable tagc, sp : character;
        variable me, md, ab, mb : integer;
        variable hk : string(1 to 64);
        variable hb : string(1 to 32);
        variable tx : bytes_t;
        variable ntx : natural;
        variable expv : bytes_t;
        variable nexp : natural;
        variable rb : std_logic_vector(7 downto 0);
        variable bad, pk : natural := 0;
        variable cyc : unsigned(31 downto 0);

        procedure send(b : std_logic_vector(7 downto 0)) is
        begin
            txd <= '0'; for i in 1 to CPB loop wait until rising_edge(clk); end loop;
            for k in 0 to 7 loop txd <= b(k); for i in 1 to CPB loop wait until rising_edge(clk); end loop; end loop;
            txd <= '1'; for i in 1 to CPB loop wait until rising_edge(clk); end loop;
        end procedure;

        procedure recv(b : out std_logic_vector(7 downto 0)) is
        begin
            wait until rxd = '0';
            for i in 1 to CPB + CPB / 2 loop wait until rising_edge(clk); end loop;
            for k in 0 to 7 loop b(k) := rxd; for i in 1 to CPB loop wait until rising_edge(clk); end loop; end loop;
        end procedure;
    begin
        wait for 1 us;
        for p in 1 to NPKT loop
            readline(f, l); read(l, tagc);
            exit when tagc = 'Z';
            read(l, me); read(l, md); read(l, ab); read(l, mb);
            tx(0) := x"52"; tx(1) := std_logic_vector(to_unsigned(me, 8)); tx(2) := std_logic_vector(to_unsigned(md, 8));
            for k in 0 to 3 loop
                tx(3 + k) := std_logic_vector(to_unsigned(ab, 32)(8 * k + 7 downto 8 * k));
                tx(7 + k) := std_logic_vector(to_unsigned(mb, 32)(8 * k + 7 downto 8 * k));
            end loop;
            ntx := 11 + 48; nexp := 0;
            loop
                readline(f, l); read(l, tagc);
                exit when tagc = 'E';
                read(l, sp);
                case tagc is
                    when 'K' => read(l, hk); for i in 0 to 31 loop tx(11 + i) := hexb(hk, i); end loop;
                    when 'I' => read(l, hb); for i in 0 to 15 loop tx(43 + i) := hexb(hb, i); end loop;
                    when 'A' | 'M' => read(l, hb); for i in 0 to 15 loop tx(ntx + i) := hexb(hb, i); end loop; ntx := ntx + 16;
                    when 'C' | 'T' => read(l, hb); for i in 0 to 15 loop expv(nexp + i) := hexb(hb, i); end loop; nexp := nexp + 16;
                    when others => null;
                end case;
            end loop;
            if me = 0 then for i in 0 to 15 loop expv(nexp + i) := x"00"; end loop; nexp := nexp + 16; end if;
            for i in 0 to ntx - 1 loop send(tx(i)); end loop;
            recv(rb);
            assert rb = x"44" report "bad reply byte" severity failure;
            for k in 0 to 3 loop recv(rb); cyc(8 * k + 7 downto 8 * k) := unsigned(rb); end loop;
            for i in 0 to nexp - 1 loop
                recv(rb);
                if rb /= expv(i) then bad := bad + 1; end if;
            end loop;
            pk := pk + 1;
            report "packet " & integer'image(pk) & ": msg " & integer'image(mb) & " bits, " &
                   integer'image(to_integer(cyc)) & " cycles" severity note;
        end loop;
        report "HWTEST SET " & integer'image(SET) & ": " & integer'image(pk) & " packets, " & integer'image(bad) & " byte mismatches" severity note;
        if bad /= 0 then report "FAIL" severity failure; end if;
        std.env.finish;
    end process;
end architecture;

-- tb_nca.vhd - packet-level test of an R4 engine against vectors from the L1 C code.
-- Generics: SET (5, 4 or 6), ARCH and D of the engine, and the vector file.
-- Also reports clock cycles per packet, so that latency can be read off.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use std.textio.all;
use work.r4_pkg.all;

entity tb_nca is
    generic (SET : natural := 5; ARCH : positive := 1; D : positive := 8;
             VEC : string := "tb/vectors/vectors_nca5.txt"; VERBOSE : natural := 0);
end entity;

architecture sim of tb_nca is
    signal clk : std_logic := '0';
    signal rst, start, mac_en, mode, din_valid, din_ready, dout_valid, tag_valid, busy : std_logic := '0';
    signal key : std_logic_vector(255 downto 0) := (others => '0');
    signal iv, din, dout, tag : blk_t := (others => '0');
    signal aad_bits, msg_bits : unsigned(31 downto 0) := (others => '0');
    signal cycle : natural := 0;

    type blk_arr is array (0 to 1100) of blk_t;

    function hex2vec(s : string; nbytes : natural) return std_logic_vector is
        variable r : std_logic_vector(8 * nbytes - 1 downto 0);
        variable v : natural;
        variable c : character;
    begin
        for i in 0 to 2 * nbytes - 1 loop
            c := s(s'low + i);
            if c >= '0' and c <= '9' then v := character'pos(c) - 48; else v := character'pos(c) - 87; end if;
            if i mod 2 = 0 then r(8 * (i / 2) + 7 downto 8 * (i / 2) + 4) := std_logic_vector(to_unsigned(v, 4));
            else r(8 * (i / 2) + 3 downto 8 * (i / 2)) := std_logic_vector(to_unsigned(v, 4)); end if;
        end loop;
        return r;
    end function;
begin
    clk <= not clk after 5 ns;
    process (clk) begin if rising_edge(clk) then cycle <= cycle + 1; end if; end process;

    g5 : if SET = 5 generate
        dut : entity work.nca5_top generic map (ARCH => ARCH, D => D)
            port map (clk, rst, start, key, iv, mac_en, mode, aad_bits, msg_bits, din, din_valid, din_ready,
                      dout, dout_valid, tag, tag_valid, busy);
    end generate;
    g4 : if SET = 4 generate
        dut : entity work.nca4_top generic map (ARCH => ARCH, D => D)
            port map (clk, rst, start, key, iv, mac_en, mode, aad_bits, msg_bits, din, din_valid, din_ready,
                      dout, dout_valid, tag, tag_valid, busy);
    end generate;
    g6 : if SET = 6 generate
        dut : entity work.nca6_top generic map (ARCH => ARCH, D => D)
            port map (clk, rst, start, key, iv, mac_en, mode, aad_bits, msg_bits, din, din_valid, din_ready,
                      dout, dout_valid, tag, tag_valid, busy);
    end generate;

    process
        file f : text open read_mode is VEC;
        variable l : line;
        variable tagc, sp : character;
        variable i_me, i_md : integer;
        variable i_ab, i_mb : integer;
        variable hk : string(1 to 64);
        variable hb : string(1 to 32);
        variable ins, exp : blk_arr;
        variable n_in, n_exp, n_out, p_idx : natural;
        variable exp_tag : blk_t;
        variable bad, pkts, c0, ccount : natural := 0;
        variable outs : blk_arr;
        variable ok : boolean;
    begin
        rst <= '1'; wait until rising_edge(clk); wait until rising_edge(clk); rst <= '0';
        loop
            readline(f, l); read(l, tagc);
            exit when tagc = 'Z';
            -- P line
            read(l, i_me); read(l, i_md); read(l, i_ab); read(l, i_mb);
            n_in := 0; n_exp := 0;
            loop
                readline(f, l); read(l, tagc);
                exit when tagc = 'E';
                read(l, sp);
                case tagc is
                    when 'K' => read(l, hk); key <= hex2vec(hk, 32);
                    when 'I' => read(l, hb); iv <= hex2vec(hb, 16);
                    when 'A' | 'M' => read(l, hb); ins(n_in) := hex2vec(hb, 16); n_in := n_in + 1;
                    when 'C' => read(l, hb); exp(n_exp) := hex2vec(hb, 16); n_exp := n_exp + 1;
                    when 'T' => read(l, hb); exp_tag := hex2vec(hb, 16);
                    when others => null;
                end case;
            end loop;
            if i_me = 1 then mac_en <= '1'; else mac_en <= '0'; end if;
            if i_md = 1 then mode <= '1'; else mode <= '0'; end if;
            aad_bits <= to_unsigned(i_ab, 32); msg_bits <= to_unsigned(i_mb, 32);
            start <= '1';
            wait until rising_edge(clk);
            c0 := cycle;
            start <= '0';
            -- feed inputs, collect outputs
            p_idx := 0; n_out := 0;
            loop
                if p_idx < n_in then din <= ins(p_idx); din_valid <= '1'; else din_valid <= '0'; end if;
                wait until rising_edge(clk);
                if dout_valid = '1' then outs(n_out) := dout; n_out := n_out + 1; end if;
                if din_valid = '1' and din_ready = '1' then p_idx := p_idx + 1; end if;
                exit when busy = '0' and p_idx = n_in;
            end loop;
            din_valid <= '0';
            ccount := cycle - c0;
            ok := n_out = n_exp;
            for i in 0 to n_exp - 1 loop
                if i < n_out and outs(i) /= exp(i) then ok := false; end if;
            end loop;
            if i_me = 1 and tag /= exp_tag then ok := false; end if;
            if not ok then
                bad := bad + 1;
                report "packet " & integer'image(pkts) & " MISMATCH (mac " & integer'image(i_me) & ", mode " &
                       integer'image(i_md) & ", aad " & integer'image(i_ab) & ", msg " & integer'image(i_mb) & " bits)" severity warning;
            end if;
            pkts := pkts + 1;
            if VERBOSE = 1 then
                report "CYCLES set " & integer'image(SET) & " arch " & integer'image(ARCH) & " D " & integer'image(D) & " msg " & integer'image(i_mb) & " bits, aad " & integer'image(i_ab) & ", mac " &
                       integer'image(i_me) & " -> " & integer'image(ccount) severity note;
            end if;
        end loop;
        report "SET " & integer'image(SET) & " ARCH " & integer'image(ARCH) & " D " & integer'image(D) & ": " &
               integer'image(pkts) & " packets, " & integer'image(bad) & " mismatches" severity note;
        if bad /= 0 then report "FAIL" severity failure; end if;
        std.env.finish;
    end process;
end architecture;

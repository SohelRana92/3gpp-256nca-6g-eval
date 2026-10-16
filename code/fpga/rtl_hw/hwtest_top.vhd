-- hwtest_top.vhd - runs one R4 engine on the Arty S7-25 and talks to a PC over the
-- USB-UART (1 Mbaud, 8N1). The PC sends a packet; the board stores it in block
-- RAM, runs the engine at full speed from RAM to RAM, counts the clock cycles,
-- and sends back the output blocks, the tag and the cycle count.
--
-- PC -> board : 'R', mac_en, mode, aad_bits (4 bytes LE), msg_bits (4 bytes LE),
--               key (32 bytes), iv (16 bytes), then all AAD and input blocks (16 bytes each)
-- board -> PC : 'D', cycles (4 bytes LE), output blocks, tag (16 bytes)
-- The engine runs at 50 MHz by default, so every design meets timing; cycle counts
-- do not depend on the clock. CLKS_PER_BIT = clock / baud rate (50 MHz / 1 Mbaud = 50).
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.r4_pkg.all;

entity hwtest_top is
    generic (SET : natural := 5; ARCH : positive := 1; D : positive := 8;
             DIV : positive := 12;                  -- system clock 600 / DIV MHz (12 -> 50 MHz)
             CLKS_PER_BIT : positive := 50);        -- system clock / 1 Mbaud
    port (
        CLK12MHZ     : in  std_logic;
        uart_txd_in  : in  std_logic;     -- from the PC
        uart_rxd_out : out std_logic;     -- to the PC
        led          : out std_logic_vector(3 downto 0)
    );
end entity;

architecture rtl of hwtest_top is
    constant DEPTH : natural := 2048;                       -- blocks per buffer (32 KiB)
    type ram_t is array (0 to DEPTH - 1) of blk_t;
    signal inbuf, outbuf : ram_t;
    signal in_q, out_q : blk_t;
    signal in_wa, in_ra, out_wa, out_ra : natural range 0 to DEPTH - 1 := 0;
    signal in_we, out_we : std_logic := '0';
    signal in_wd : blk_t;

    signal clk, locked, rst : std_logic;
    signal rx_d : std_logic_vector(7 downto 0);
    signal rx_v, tx_send, tx_ready : std_logic;
    signal tx_d : std_logic_vector(7 downto 0);

    type st_t is (S_CMD, S_HDR, S_DATA, S_PRE, S_RUN, S_TXH, S_TXLOAD, S_TXWAIT, S_TXLD2, S_TXBLK, S_TXTAG);
    signal st : st_t := S_CMD;
    signal hdr : std_logic_vector(58 * 8 - 1 downto 0);
    signal hcnt : natural range 0 to 58 := 0;
    signal bcnt : natural range 0 to 16 := 0;
    signal acc : blk_t;
    signal n_in, n_out, blk_i : natural range 0 to DEPTH := 0;
    signal ptr : natural range 0 to DEPTH := 0;
    signal cycles : unsigned(31 downto 0);
    signal seen_busy : std_logic := '0';
    signal txbuf : blk_t;
    signal tx_left : natural range 0 to 16 := 0;
    signal tagr : blk_t;
    signal heartbeat : unsigned(26 downto 0) := (others => '0');

    -- engine
    signal e_start, e_mac, e_mode, e_din_valid, e_din_ready, e_dout_valid, e_tag_valid, e_busy : std_logic;
    signal e_key : std_logic_vector(255 downto 0);
    signal e_iv, e_din, e_dout, e_tag : blk_t;
    signal e_aad, e_msg : unsigned(31 downto 0);
    signal consume : std_logic;
    signal next_ptr : natural range 0 to DEPTH;

    function nblocks(bits : unsigned(31 downto 0)) return natural is
    begin
        return to_integer((bits + 127) / 128);
    end function;
begin
    cg : entity work.clkgen generic map (DIV => DIV) port map (CLK12MHZ, clk, locked);
    rst <= not locked;
    urx : entity work.uart_rx generic map (CLKS_PER_BIT) port map (clk, uart_txd_in, rx_d, rx_v);
    utx : entity work.uart_tx generic map (CLKS_PER_BIT) port map (clk, tx_d, tx_send, tx_ready, uart_rxd_out);

    -- header fields
    e_mac  <= hdr(0);
    e_mode <= hdr(8);
    e_aad  <= unsigned(hdr(6 * 8 - 1 downto 2 * 8));
    e_msg  <= unsigned(hdr(10 * 8 - 1 downto 6 * 8));
    e_key  <= hdr(42 * 8 - 1 downto 10 * 8);
    e_iv   <= hdr(58 * 8 - 1 downto 42 * 8);

    -- block RAMs (one write port, one registered read port each)
    process (clk) begin
        if rising_edge(clk) then
            if in_we = '1' then inbuf(in_wa) <= in_wd; end if;
            in_q <= inbuf(in_ra);
            if out_we = '1' then outbuf(out_wa) <= e_dout; end if;
            out_q <= outbuf(out_ra);
        end if;
    end process;

    e_din <= in_q;
    e_din_valid <= '1' when st = S_RUN and ptr < n_in else '0';
    consume <= e_din_valid and e_din_ready;
    next_ptr <= ptr + 1 when consume = '1' else ptr;
    in_ra <= next_ptr mod DEPTH when st = S_RUN else 0;
    out_we <= e_dout_valid;

    g5 : if SET = 5 generate
        e : entity work.nca5_top generic map (ARCH => ARCH, D => D)
            port map (clk, rst, e_start, e_key, e_iv, e_mac, e_mode, e_aad, e_msg, e_din, e_din_valid, e_din_ready,
                      e_dout, e_dout_valid, e_tag, e_tag_valid, e_busy);
    end generate;
    g4 : if SET = 4 generate
        e : entity work.nca4_top generic map (ARCH => ARCH, D => D)
            port map (clk, rst, e_start, e_key, e_iv, e_mac, e_mode, e_aad, e_msg, e_din, e_din_valid, e_din_ready,
                      e_dout, e_dout_valid, e_tag, e_tag_valid, e_busy);
    end generate;
    g6 : if SET = 6 generate
        e : entity work.nca6_top generic map (ARCH => ARCH, D => D)
            port map (clk, rst, e_start, e_key, e_iv, e_mac, e_mode, e_aad, e_msg, e_din, e_din_valid, e_din_ready,
                      e_dout, e_dout_valid, e_tag, e_tag_valid, e_busy);
    end generate;

    led(0) <= heartbeat(26);
    led(1) <= '1' when st = S_RUN else '0';
    led(2) <= '1' when st = S_HDR or st = S_DATA else '0';
    led(3) <= locked;

    process (clk)
    begin
        if rising_edge(clk) then
            heartbeat <= heartbeat + 1;
            in_we <= '0'; tx_send <= '0'; e_start <= '0';
            if rst = '1' then
                st <= S_CMD;
            else
                if e_dout_valid = '1' then out_wa <= (out_wa + 1) mod DEPTH; end if;
                if e_tag_valid = '1' then tagr <= e_tag; end if;
                case st is
                    when S_CMD =>
                        if rx_v = '1' and rx_d = x"52" then st <= S_HDR; hcnt <= 0; end if;      -- 'R'
                    when S_HDR =>
                        if rx_v = '1' then
                            hdr(8 * hcnt + 7 downto 8 * hcnt) <= rx_d;
                            if hcnt = 57 then st <= S_DATA; bcnt <= 0; blk_i <= 0; else hcnt <= hcnt + 1; end if;
                        end if;
                    when S_DATA =>
                        n_in <= nblocks(e_aad) + nblocks(e_msg);
                        n_out <= nblocks(e_msg);
                        if blk_i = nblocks(e_aad) + nblocks(e_msg) then
                            st <= S_PRE;
                        elsif rx_v = '1' then
                            acc(8 * bcnt + 7 downto 8 * bcnt) <= rx_d;
                            if bcnt = 15 then
                                in_wd <= rx_d & acc(119 downto 0); in_wa <= blk_i; in_we <= '1';
                                blk_i <= blk_i + 1; bcnt <= 0;
                            else
                                bcnt <= bcnt + 1;
                            end if;
                        end if;
                    when S_PRE =>                            -- in_ra = 0 now; in_q = block 0 next clock
                        ptr <= 0; out_wa <= 0; cycles <= (others => '0'); seen_busy <= '0';
                        tagr <= (others => '0');
                        e_start <= '1';
                        st <= S_RUN;
                    when S_RUN =>
                        ptr <= next_ptr;
                        cycles <= cycles + 1;
                        if e_busy = '1' then seen_busy <= '1'; end if;
                        if seen_busy = '1' and e_busy = '0' then st <= S_TXH; hcnt <= 0; end if;
                    when S_TXH =>                            -- 'D' + cycles
                        if tx_ready = '1' and tx_send = '0' then
                            if hcnt = 0 then tx_d <= x"44";
                            else tx_d <= std_logic_vector(cycles(8 * hcnt - 1 downto 8 * hcnt - 8)); end if;
                            tx_send <= '1';
                            if hcnt = 4 then st <= S_TXLOAD; blk_i <= 0; else hcnt <= hcnt + 1; end if;
                        end if;
                    when S_TXLOAD =>
                        if blk_i = n_out then
                            txbuf <= tagr; tx_left <= 16; st <= S_TXTAG;
                        else
                            out_ra <= blk_i; st <= S_TXWAIT;
                        end if;
                    when S_TXWAIT =>                         -- out_q becomes valid after this clock
                        st <= S_TXLD2;
                    when S_TXLD2 =>
                        txbuf <= out_q; tx_left <= 16; st <= S_TXBLK;
                    when S_TXBLK =>
                        if tx_left = 0 then
                            blk_i <= blk_i + 1; st <= S_TXLOAD;
                        elsif tx_ready = '1' and tx_send = '0' then
                            tx_d <= txbuf(7 downto 0); txbuf <= x"00" & txbuf(127 downto 8); tx_send <= '1';
                            tx_left <= tx_left - 1;
                        end if;
                    when S_TXTAG =>
                        if tx_ready = '1' and tx_send = '0' and tx_left /= 0 then
                            tx_d <= txbuf(7 downto 0); txbuf <= x"00" & txbuf(127 downto 8); tx_send <= '1';
                            tx_left <= tx_left - 1;
                        elsif tx_left = 0 and tx_ready = '1' and tx_send = '0' then
                            st <= S_CMD;
                        end if;
                end case;
            end if;
        end if;
    end process;
end architecture;

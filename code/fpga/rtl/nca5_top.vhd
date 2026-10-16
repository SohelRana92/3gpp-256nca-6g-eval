-- nca5_top.vhd - 256-NEA5 / NIA5 / NCA5 engine: AES-256 KSG + 256-AEAD1.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.r4_pkg.all;

entity nca5_top is
    generic (ARCH : positive := 1; D : positive := 8);    -- ARCH = AES rounds per clock (1 or 2)
    port (
        clk, rst   : in  std_logic;
        start      : in  std_logic;
        key        : in  std_logic_vector(255 downto 0);
        iv         : in  blk_t;
        mac_en     : in  std_logic;
        mode       : in  std_logic;
        aad_bits   : in  unsigned(31 downto 0);
        msg_bits   : in  unsigned(31 downto 0);
        din        : in  blk_t;
        din_valid  : in  std_logic;
        din_ready  : out std_logic;
        dout       : out blk_t;
        dout_valid : out std_logic;
        tag        : out blk_t;
        tag_valid  : out std_logic;
        busy       : out std_logic
    );
end entity;

architecture rtl of nca5_top is
    signal k_init, k_valid, k_take : std_logic;
    signal k_ks : blk_t;
begin
    ksg : entity work.ksg_aes generic map (RPC => ARCH)
        port map (clk, rst, k_init, key, iv, mac_en, k_take, k_valid, k_ks);
    ctl : entity work.aead1_ctrl generic map (D => D)
        port map (clk, rst, start, mac_en, mode, aad_bits, msg_bits, din, din_valid, din_ready,
                  dout, dout_valid, tag, tag_valid, busy, k_init, k_valid, k_ks, k_take);
end architecture;

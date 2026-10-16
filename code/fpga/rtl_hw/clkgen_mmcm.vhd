-- clkgen_mmcm.vhd - system clock from the 12 MHz board clock (VCO 600 MHz). Vivado only.
-- DIV = 6 -> 100 MHz, 8 -> 75 MHz, 12 -> 50 MHz.
library ieee;
use ieee.std_logic_1164.all;
library unisim;
use unisim.vcomponents.all;

entity clkgen is
    generic (DIV : positive := 12);
    port (clk12 : in std_logic; clk : out std_logic; locked : out std_logic);
end entity;

architecture rtl of clkgen is
    signal fb, c0 : std_logic;
begin
    mmcm : MMCME2_BASE
        generic map (CLKIN1_PERIOD => 83.333, CLKFBOUT_MULT_F => 50.0, DIVCLK_DIVIDE => 1, CLKOUT0_DIVIDE_F => real(DIV))
        port map (CLKIN1 => clk12, CLKFBIN => fb, CLKFBOUT => fb, CLKOUT0 => c0, LOCKED => locked,
                  PWRDWN => '0', RST => '0',
                  CLKOUT0B => open, CLKOUT1 => open, CLKOUT1B => open, CLKOUT2 => open, CLKOUT2B => open,
                  CLKOUT3 => open, CLKOUT3B => open, CLKOUT4 => open, CLKOUT5 => open, CLKOUT6 => open,
                  CLKFBOUTB => open);
    bufg_i : BUFG port map (I => c0, O => clk);
end architecture;

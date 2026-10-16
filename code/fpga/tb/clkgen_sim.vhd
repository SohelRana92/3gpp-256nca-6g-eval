-- clkgen_sim.vhd - simulation stand-in for the MMCM: passes the clock through.
library ieee;
use ieee.std_logic_1164.all;
entity clkgen is
    generic (DIV : positive := 12);
    port (clk12 : in std_logic; clk : out std_logic; locked : out std_logic);
end entity;
architecture sim of clkgen is
begin
    clk <= clk12; locked <= '1';
end architecture;

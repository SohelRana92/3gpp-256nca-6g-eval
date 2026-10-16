-- uart.vhd - minimal 8N1 UART receiver and transmitter for the hardware test.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity uart_rx is
    generic (CLKS_PER_BIT : positive := 100);
    port (clk : in std_logic; rx : in std_logic; data : out std_logic_vector(7 downto 0); valid : out std_logic);
end entity;

architecture rtl of uart_rx is
    signal s1, s2 : std_logic := '1';
    signal cnt    : natural range 0 to CLKS_PER_BIT - 1 := 0;
    signal bitn   : natural range 0 to 9 := 0;
    signal busy   : std_logic := '0';
    signal sh     : std_logic_vector(7 downto 0) := (others => '0');
begin
    process (clk)
    begin
        if rising_edge(clk) then
            s1 <= rx; s2 <= s1; valid <= '0';
            if busy = '0' then
                if s2 = '0' then busy <= '1'; cnt <= CLKS_PER_BIT / 2; bitn <= 0; end if;   -- start bit edge
            elsif cnt /= 0 then
                cnt <= cnt - 1;
            else
                cnt <= CLKS_PER_BIT - 1;
                if bitn = 0 then
                    if s2 /= '0' then busy <= '0'; end if;                                 -- false start
                    bitn <= 1;
                elsif bitn <= 8 then
                    sh <= s2 & sh(7 downto 1); bitn <= bitn + 1;
                else
                    busy <= '0';
                    if s2 = '1' then data <= sh; valid <= '1'; end if;                      -- stop bit
                end if;
            end if;
        end if;
    end process;
end architecture;

library ieee;
use ieee.std_logic_1164.all;

entity uart_tx is
    generic (CLKS_PER_BIT : positive := 100);
    port (clk : in std_logic; data : in std_logic_vector(7 downto 0); send : in std_logic;
          ready : out std_logic; tx : out std_logic);
end entity;

architecture rtl of uart_tx is
    signal sh   : std_logic_vector(9 downto 0) := (others => '1');
    signal cnt  : natural range 0 to CLKS_PER_BIT - 1 := 0;
    signal left : natural range 0 to 10 := 0;
begin
    ready <= '1' when left = 0 else '0';
    tx <= sh(0);
    process (clk)
    begin
        if rising_edge(clk) then
            if left = 0 then
                if send = '1' then sh <= '1' & data & '0'; left <= 10; cnt <= CLKS_PER_BIT - 1; end if;
            elsif cnt /= 0 then
                cnt <= cnt - 1;
            else
                cnt <= CLKS_PER_BIT - 1;
                sh <= '1' & sh(9 downto 1);
                left <= left - 1;
            end if;
        end if;
    end process;
end architecture;

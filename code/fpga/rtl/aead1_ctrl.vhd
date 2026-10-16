-- aead1_ctrl.vhd - 256-AEAD1 packet engine (TS 35.243, clause 5.2), shared by
-- all three sets. It drives a keystream generator (KSG) and the POLYVAL MAC.
--
-- One packet: start (with key, IV, mode, lengths in bits), then the AAD blocks
-- and the input blocks on din (16 bytes each, zero-padded), then the tag.
--   mac_en = '0' : 256-NEA (no H, Q, P; no tag)
--   mac_en = '1' : 256-NIA (input passed as AAD, msg_bits = 0) or 256-NCA
--   mode   = '0' encrypt (MAC over the output), '1' decrypt (MAC over the input)
-- The unused bits of the last block are cleared, as in ClearBits.
-- The output port has no back-pressure: dout is valid for one clock.
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.r4_pkg.all;

entity aead1_ctrl is
    generic (D : positive := 8);                     -- POLYVAL digit size
    port (
        clk, rst   : in  std_logic;
        start      : in  std_logic;
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
        busy       : out std_logic;
        -- KSG side
        ksg_init   : out std_logic;
        ksg_valid  : in  std_logic;
        ksg_ks     : in  blk_t;
        ksg_take   : out std_logic
    );
end entity;

architecture rtl of aead1_ctrl is
    type state_t is (S_IDLE, S_HQP, S_AAD, S_MSG, S_FIN, S_WAIT);
    signal st : state_t := S_IDLE;
    signal m_en, md : std_logic;
    signal a_left, m_left, a_len, m_len : unsigned(31 downto 0);
    signal nh : natural range 0 to 3;

    signal mac_clear, mac_load, mac_start, mac_final, mac_ready, mac_done : std_logic;
    signal mac_x, mac_a : blk_t;

    signal acc_aad, acc_msg : std_logic;
    signal cur_mask, out_blk, in_masked : blk_t;
    signal in_hqp, in_fin : std_logic;

    function mask_for(left : unsigned(31 downto 0)) return blk_t is
    begin
        if left >= 128 then return (others => '1'); end if;
        return bit_mask(to_integer(left(6 downto 0)));
    end function;
begin
    mac : entity work.polyval_mac generic map (D => D)
        port map (clk, rst, mac_clear, mac_load, ksg_ks, mac_start, mac_final, mac_x, mac_ready, mac_done, mac_a);

    cur_mask  <= mask_for(a_left) when st = S_AAD else mask_for(m_left);
    in_masked <= din and cur_mask;
    out_blk   <= (din xor ksg_ks) and cur_mask;

    acc_aad <= '1' when st = S_AAD and a_left /= 0 and din_valid = '1' and mac_ready = '1' else '0';
    acc_msg <= '1' when st = S_MSG and m_left /= 0 and din_valid = '1' and ksg_valid = '1'
                        and (mac_ready = '1' or m_en = '0') else '0';

    din_ready <= '1' when (st = S_AAD and a_left /= 0 and mac_ready = '1') or
                          (st = S_MSG and m_left /= 0 and ksg_valid = '1' and (mac_ready = '1' or m_en = '0')) else '0';

    dout       <= out_blk;
    dout_valid <= acc_msg;
    in_hqp     <= '1' when st = S_HQP else '0';
    in_fin     <= '1' when st = S_FIN else '0';
    ksg_take   <= acc_msg or (ksg_valid and in_hqp);
    mac_load   <= ksg_valid and in_hqp;
    mac_clear  <= start when st = S_IDLE else '0';
    ksg_init   <= start when st = S_IDLE else '0';

    mac_start <= acc_aad or (acc_msg and m_en) or (in_fin and mac_ready);
    mac_final <= '1' when st = S_FIN else '0';
    mac_x <= in_masked when st = S_AAD else
             out_blk   when st = S_MSG and md = '0' else
             in_masked when st = S_MSG else
             x"00000000" & std_logic_vector(a_len) & x"00000000" & std_logic_vector(m_len);     -- L: s_len in bytes 0..3, a_len in bytes 8..11

    tag  <= mac_a;
    busy <= '0' when st = S_IDLE else '1';

    process (clk)
        function dec(v : unsigned(31 downto 0)) return unsigned is
        begin
            if v >= 128 then return v - 128; else return to_unsigned(0, 32); end if;
        end function;
    begin
        if rising_edge(clk) then
            tag_valid <= '0';
            if rst = '1' then
                st <= S_IDLE;
            else
                case st is
                    when S_IDLE =>
                        if start = '1' then
                            m_en <= mac_en; md <= mode;
                            a_left <= aad_bits; m_left <= msg_bits; a_len <= aad_bits; m_len <= msg_bits;
                            nh <= 0;
                            if mac_en = '1' then st <= S_HQP; else st <= S_MSG; end if;
                        end if;
                    when S_HQP =>
                        if ksg_valid = '1' then
                            if nh = 2 then st <= S_AAD; else nh <= nh + 1; end if;
                        end if;
                    when S_AAD =>
                        if a_left = 0 then st <= S_MSG;
                        elsif acc_aad = '1' then a_left <= dec(a_left); end if;
                    when S_MSG =>
                        if m_left = 0 then
                            if m_en = '1' then st <= S_FIN; else st <= S_IDLE; end if;
                        elsif acc_msg = '1' then m_left <= dec(m_left); end if;
                    when S_FIN =>
                        if mac_ready = '1' then st <= S_WAIT; end if;
                    when S_WAIT =>
                        if mac_done = '1' then tag_valid <= '1'; st <= S_IDLE; end if;
                end case;
            end if;
        end if;
    end process;
end architecture;

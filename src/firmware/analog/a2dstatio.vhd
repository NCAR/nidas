--------------------------------------------------------------------------------
-- Copyright (c) 1995-2003 Xilinx, Inc.
-- All Right Reserved.
--------------------------------------------------------------------------------
--   ____  ____ 
--  /   /\/   / 
-- /___/  \  /    Vendor: Xilinx 
-- \   \   \/     Version : 7.1.04i
--  \   \         Application : sch2vhdl
--  /   /         Filename : a2dstatio.vhf
-- /___/   /\     Timestamp : 02/10/2006 14:47:29
-- \   \  /  \ 
--  \___\/\___\ 
--
--Command: C:/Xilinx/bin/nt/sch2vhdl.exe -intstyle ise -family xc9500 -flat -suppress -w a2dstatio.sch a2dstatio.vhf
--Design Name: a2dstatio
--Device: xc9500
--Purpose:
--    This vhdl netlist is translated from an ECS schematic. It can be 
--    synthesis and simulted, but it should not be modified. 
--

library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity FTCE_MXILINX_a2dstatio is
   port ( C   : in    std_logic; 
          CE  : in    std_logic; 
          CLR : in    std_logic; 
          T   : in    std_logic; 
          Q   : out   std_logic);
end FTCE_MXILINX_a2dstatio;

architecture BEHAVIORAL of FTCE_MXILINX_a2dstatio is
   attribute BOX_TYPE       : string ;
   signal TQ      : std_logic;
   signal Q_DUMMY : std_logic;
   component XOR2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of XOR2 : component is "BLACK_BOX";
   
   component FDCE
      port ( C   : in    std_logic; 
             CE  : in    std_logic; 
             CLR : in    std_logic; 
             D   : in    std_logic; 
             Q   : out   std_logic);
   end component;
   attribute BOX_TYPE of FDCE : component is "BLACK_BOX";
   
begin
   Q <= Q_DUMMY;
   I_36_32 : XOR2
      port map (I0=>T,
                I1=>Q_DUMMY,
                O=>TQ);
   
   I_36_35 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>TQ,
                Q=>Q_DUMMY);
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity CB4CE_MXILINX_a2dstatio is
   port ( C   : in    std_logic; 
          CE  : in    std_logic; 
          CLR : in    std_logic; 
          CEO : out   std_logic; 
          Q0  : out   std_logic; 
          Q1  : out   std_logic; 
          Q2  : out   std_logic; 
          Q3  : out   std_logic; 
          TC  : out   std_logic);
end CB4CE_MXILINX_a2dstatio;

architecture BEHAVIORAL of CB4CE_MXILINX_a2dstatio is
   attribute BOX_TYPE   : string ;
   attribute HU_SET     : string ;
   signal T2       : std_logic;
   signal T3       : std_logic;
   signal XLXN_1   : std_logic;
   signal Q0_DUMMY : std_logic;
   signal Q1_DUMMY : std_logic;
   signal Q2_DUMMY : std_logic;
   signal Q3_DUMMY : std_logic;
   signal TC_DUMMY : std_logic;
   component AND4
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             I3 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND4 : component is "BLACK_BOX";
   
   component AND3
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND3 : component is "BLACK_BOX";
   
   component AND2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND2 : component is "BLACK_BOX";
   
   component VCC
      port ( P : out   std_logic);
   end component;
   attribute BOX_TYPE of VCC : component is "BLACK_BOX";
   
   component FTCE_MXILINX_a2dstatio
      port ( C   : in    std_logic; 
             CE  : in    std_logic; 
             CLR : in    std_logic; 
             T   : in    std_logic; 
             Q   : out   std_logic);
   end component;
   
   attribute HU_SET of U0 : label is "U0_0";
   attribute HU_SET of U1 : label is "U1_1";
   attribute HU_SET of U2 : label is "U2_2";
   attribute HU_SET of U3 : label is "U3_3";
begin
   Q0 <= Q0_DUMMY;
   Q1 <= Q1_DUMMY;
   Q2 <= Q2_DUMMY;
   Q3 <= Q3_DUMMY;
   TC <= TC_DUMMY;
   I_36_31 : AND4
      port map (I0=>Q3_DUMMY,
                I1=>Q2_DUMMY,
                I2=>Q1_DUMMY,
                I3=>Q0_DUMMY,
                O=>TC_DUMMY);
   
   I_36_32 : AND3
      port map (I0=>Q2_DUMMY,
                I1=>Q1_DUMMY,
                I2=>Q0_DUMMY,
                O=>T3);
   
   I_36_33 : AND2
      port map (I0=>Q1_DUMMY,
                I1=>Q0_DUMMY,
                O=>T2);
   
   I_36_58 : VCC
      port map (P=>XLXN_1);
   
   I_36_67 : AND2
      port map (I0=>CE,
                I1=>TC_DUMMY,
                O=>CEO);
   
   U0 : FTCE_MXILINX_a2dstatio
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>XLXN_1,
                Q=>Q0_DUMMY);
   
   U1 : FTCE_MXILINX_a2dstatio
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>Q0_DUMMY,
                Q=>Q1_DUMMY);
   
   U2 : FTCE_MXILINX_a2dstatio
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>T2,
                Q=>Q2_DUMMY);
   
   U3 : FTCE_MXILINX_a2dstatio
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>T3,
                Q=>Q3_DUMMY);
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity BUFE16_MXILINX_a2dstatio is
   port ( E : in    std_logic; 
          I : in    std_logic_vector (15 downto 0); 
          O : out   std_logic_vector (15 downto 0));
end BUFE16_MXILINX_a2dstatio;

architecture BEHAVIORAL of BUFE16_MXILINX_a2dstatio is
   attribute BOX_TYPE   : string ;
   component BUFE
      port ( E : in    std_logic; 
             I : in    std_logic; 
             O : out   std_logic);
   end component;
   attribute BOX_TYPE of BUFE : component is "BLACK_BOX";
   
begin
   I_36_30 : BUFE
      port map (E=>E,
                I=>I(8),
                O=>O(8));
   
   I_36_31 : BUFE
      port map (E=>E,
                I=>I(9),
                O=>O(9));
   
   I_36_32 : BUFE
      port map (E=>E,
                I=>I(10),
                O=>O(10));
   
   I_36_33 : BUFE
      port map (E=>E,
                I=>I(11),
                O=>O(11));
   
   I_36_34 : BUFE
      port map (E=>E,
                I=>I(15),
                O=>O(15));
   
   I_36_35 : BUFE
      port map (E=>E,
                I=>I(14),
                O=>O(14));
   
   I_36_36 : BUFE
      port map (E=>E,
                I=>I(13),
                O=>O(13));
   
   I_36_37 : BUFE
      port map (E=>E,
                I=>I(12),
                O=>O(12));
   
   I_36_38 : BUFE
      port map (E=>E,
                I=>I(6),
                O=>O(6));
   
   I_36_39 : BUFE
      port map (E=>E,
                I=>I(7),
                O=>O(7));
   
   I_36_40 : BUFE
      port map (E=>E,
                I=>I(0),
                O=>O(0));
   
   I_36_41 : BUFE
      port map (E=>E,
                I=>I(1),
                O=>O(1));
   
   I_36_42 : BUFE
      port map (E=>E,
                I=>I(2),
                O=>O(2));
   
   I_36_43 : BUFE
      port map (E=>E,
                I=>I(3),
                O=>O(3));
   
   I_36_44 : BUFE
      port map (E=>E,
                I=>I(4),
                O=>O(4));
   
   I_36_45 : BUFE
      port map (E=>E,
                I=>I(5),
                O=>O(5));
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity a2dstatio is
   port ( A2DIOEBL : in    std_logic; 
          PLLOUT   : in    std_logic; 
          SIOR     : in    std_logic; 
          SIOW     : in    std_logic; 
			 A2DRS    : in    std_logic;
          FIFOCTL  : in    std_logic_vector (7 downto 0); 			 
          PLLDBN   : out   std_logic; 
--          TEST45   : out   std_logic; 
          A2DBUS   : inout std_logic_vector (15 downto 0); 
          BSD      : inout std_logic_vector (15 downto 0));
end a2dstatio;

architecture BEHAVIORAL of a2dstatio is
   attribute HU_SET     : string ;
   attribute BOX_TYPE   : string ;
--	signal Latch_A2DBUS: std_logic_vector(15 downto 0);
   signal XLXN_199 : std_logic;
   signal XLXN_200 : std_logic;
   signal XLXN_201 : std_logic;
   signal XLXN_202 : std_logic;
   signal XLXN_203 : std_logic;
   signal XLXN_238 : std_logic;
   signal XLXN_239 : std_logic;
	signal my_data  : std_logic_vector(15 downto 0);
--   component FD16_MXILINX_a2dstatio
--     port ( C   : in    std_logic; 
--            D   : in    std_logic_vector (15 downto 0); 
--            Q   : out   std_logic_vector (15 downto 0));
--   end component;
   component BUFE16_MXILINX_a2dstatio
      port ( E : in    std_logic; 
             I : in    std_logic_vector (15 downto 0); 
             O : out   std_logic_vector (15 downto 0));
   end component;
   
   component AND2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND2 : component is "BLACK_BOX";
   
   component CB4CE_MXILINX_a2dstatio
      port ( C   : in    std_logic; 
             CE  : in    std_logic; 
             CLR : in    std_logic; 
             CEO : out   std_logic; 
             Q0  : out   std_logic; 
             Q1  : out   std_logic; 
             Q2  : out   std_logic; 
             Q3  : out   std_logic; 
             TC  : out   std_logic);
   end component;
   
   component VCC
      port ( P : out   std_logic);
   end component;
   attribute BOX_TYPE of VCC : component is "BLACK_BOX";
   
   component GND
      port ( G : out   std_logic);
   end component;
   attribute BOX_TYPE of GND : component is "BLACK_BOX";
   
   component BUF
      port ( I : in    std_logic; 
             O : out   std_logic);
   end component;
   attribute BOX_TYPE of BUF : component is "BLACK_BOX";
   
   attribute HU_SET of XLXI_17 : label is "XLXI_17_4";
   attribute HU_SET of XLXI_84 : label is "XLXI_84_5";
   attribute HU_SET of XLXI_160 : label is "XLXI_160_6";
   attribute HU_SET of XLXI_166 : label is "XLXI_166_7";
begin

  process(A2DRS)
  begin
    if A2DRS = '1' then
	   my_data <= X"AAAA";
	 else
	   my_data <= X"5555";
	 end if;
  end process;
	 
--   MY_XLXI : FD16_MXILINX_a2dstatio
--      port map (C=>PLLOUT,
--                D(15 downto 0)=>A2DBUS(15 downto 0),
--                Q(15 downto 0)=>Latch_A2DBUS(15 downto 0));

   XLXI_17 : BUFE16_MXILINX_a2dstatio
      port map (E=>XLXN_238,
--                I(15 downto 0)=>X"AAAA",
                I(15 downto 0)=>A2DBUS(15 downto 0),
                O(15 downto 0)=>BSD(15 downto 0));
   
   XLXI_18 : AND2
      port map (I0=>SIOR,
                I1=>A2DIOEBL,
                O=>XLXN_238);
   
   XLXI_84 : BUFE16_MXILINX_a2dstatio
      port map (E=>XLXN_239,
--      port map (E=>FIFOCTL(1),
--                I(15 downto 0)=>my_data(15 downto 0),
                I(15 downto 0)=>BSD(15 downto 0),
                O(15 downto 0)=>A2DBUS(15 downto 0));
   
   XLXI_160 : CB4CE_MXILINX_a2dstatio
      port map (C=>PLLOUT,
                CE=>XLXN_200,
                CLR=>XLXN_199,
                CEO=>open,
                Q0=>open,
                Q1=>open,
                Q2=>open,
                Q3=>XLXN_203,
                TC=>open);
   
   XLXI_161 : VCC
      port map (P=>XLXN_200);
   
   XLXI_162 : GND
      port map (G=>XLXN_199);
   
   XLXI_164 : VCC
      port map (P=>XLXN_201);
   
   XLXI_165 : GND
      port map (G=>XLXN_202);
   
   XLXI_166 : CB4CE_MXILINX_a2dstatio
      port map (C=>XLXN_203,
                CE=>XLXN_201,
                CLR=>XLXN_202,
                CEO=>open,
                Q0=>open,
                Q1=>open,
                Q2=>PLLDBN,
                Q3=>open,
                TC=>open);
   
--   XLXI_185 : BUF
--      port map (I=>XLXN_238,
--                O=>TEST45);
   
   XLXI_186 : AND2
      port map (I0=>A2DIOEBL,
                I1=>SIOW,
                O=>XLXN_239);
   
end BEHAVIORAL;



--------------------------------------------------------------------------------
-- Copyright (c) 1995-2003 Xilinx, Inc.
-- All Right Reserved.
--------------------------------------------------------------------------------
--   ____  ____ 
--  /   /\/   / 
-- /___/  \  /    Vendor: Xilinx 
-- \   \   \/     Version : 7.1.04i
--  \   \         Application : sch2vhdl
--  /   /         Filename : a2dtiming.vhf
-- /___/   /\     Timestamp : 02/10/2006 15:13:44
-- \   \  /  \ 
--  \___\/\___\ 
--
--Command: C:/Xilinx/bin/nt/sch2vhdl.exe -intstyle ise -family xc9500 -flat -suppress -w a2dtiming.sch a2dtiming.vhf
--Design Name: a2dtiming
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

entity FD_MXILINX_a2dtiming is
   port ( C : in    std_logic; 
          D : in    std_logic; 
          Q : out   std_logic);
end FD_MXILINX_a2dtiming;

architecture BEHAVIORAL of FD_MXILINX_a2dtiming is
   attribute BOX_TYPE   : string ;
   signal XLXN_4 : std_logic;
   component GND
      port ( G : out   std_logic);
   end component;
   attribute BOX_TYPE of GND : component is "BLACK_BOX";
   
   component FDCP
      port ( C   : in    std_logic; 
             CLR : in    std_logic; 
             D   : in    std_logic; 
             PRE : in    std_logic; 
             Q   : out   std_logic);
   end component;
   attribute BOX_TYPE of FDCP : component is "BLACK_BOX";
   
begin
   I_36_43 : GND
      port map (G=>XLXN_4);
   
   U0 : FDCP
      port map (C=>C,
                CLR=>XLXN_4,
                D=>D,
                PRE=>XLXN_4,
                Q=>Q);
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity FDC_MXILINX_a2dtiming is
   port ( C   : in    std_logic; 
          CLR : in    std_logic; 
          D   : in    std_logic; 
          Q   : out   std_logic);
end FDC_MXILINX_a2dtiming;

architecture BEHAVIORAL of FDC_MXILINX_a2dtiming is
   attribute BOX_TYPE   : string ;
   signal XLXN_5 : std_logic;
   component GND
      port ( G : out   std_logic);
   end component;
   attribute BOX_TYPE of GND : component is "BLACK_BOX";
   
   component FDCP
      port ( C   : in    std_logic; 
             CLR : in    std_logic; 
             D   : in    std_logic; 
             PRE : in    std_logic; 
             Q   : out   std_logic);
   end component;
   attribute BOX_TYPE of FDCP : component is "BLACK_BOX";
   
begin
   I_36_55 : GND
      port map (G=>XLXN_5);
   
   U0 : FDCP
      port map (C=>C,
                CLR=>CLR,
                D=>D,
                PRE=>XLXN_5,
                Q=>Q);
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity FTCE_MXILINX_a2dtiming is
   port ( C   : in    std_logic; 
          CE  : in    std_logic; 
          CLR : in    std_logic; 
          T   : in    std_logic; 
          Q   : out   std_logic);
end FTCE_MXILINX_a2dtiming;

architecture BEHAVIORAL of FTCE_MXILINX_a2dtiming is
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

entity CB8CE_MXILINX_a2dtiming is
   port ( C   : in    std_logic; 
          CE  : in    std_logic; 
          CLR : in    std_logic; 
          CEO : out   std_logic; 
          Q   : out   std_logic_vector (7 downto 0); 
          TC  : out   std_logic);
end CB8CE_MXILINX_a2dtiming;

architecture BEHAVIORAL of CB8CE_MXILINX_a2dtiming is
   attribute BOX_TYPE   : string ;
   attribute HU_SET     : string ;
   signal T2       : std_logic;
   signal T3       : std_logic;
   signal T4       : std_logic;
   signal T5       : std_logic;
   signal T6       : std_logic;
   signal T7       : std_logic;
   signal XLXN_1   : std_logic;
   signal Q_DUMMY  : std_logic_vector (7 downto 0);
   signal TC_DUMMY : std_logic;
   component AND5
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             I3 : in    std_logic; 
             I4 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND5 : component is "BLACK_BOX";
   
   component AND2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND2 : component is "BLACK_BOX";
   
   component AND3
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND3 : component is "BLACK_BOX";
   
   component AND4
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             I3 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND4 : component is "BLACK_BOX";
   
   component VCC
      port ( P : out   std_logic);
   end component;
   attribute BOX_TYPE of VCC : component is "BLACK_BOX";
   
   component FTCE_MXILINX_a2dtiming
      port ( C   : in    std_logic; 
             CE  : in    std_logic; 
             CLR : in    std_logic; 
             T   : in    std_logic; 
             Q   : out   std_logic);
   end component;
   
   attribute HU_SET of Q0 : label is "Q0_6";
   attribute HU_SET of Q1 : label is "Q1_7";
   attribute HU_SET of Q2 : label is "Q2_3";
   attribute HU_SET of Q3 : label is "Q3_4";
   attribute HU_SET of Q4 : label is "Q4_5";
   attribute HU_SET of Q5 : label is "Q5_2";
   attribute HU_SET of Q6 : label is "Q6_1";
   attribute HU_SET of Q7 : label is "Q7_0";
begin
   Q(7 downto 0) <= Q_DUMMY(7 downto 0);
   TC <= TC_DUMMY;
   I_36_1 : AND5
      port map (I0=>Q_DUMMY(7),
                I1=>Q_DUMMY(6),
                I2=>Q_DUMMY(5),
                I3=>Q_DUMMY(4),
                I4=>T4,
                O=>TC_DUMMY);
   
   I_36_2 : AND2
      port map (I0=>Q_DUMMY(4),
                I1=>T4,
                O=>T5);
   
   I_36_11 : AND3
      port map (I0=>Q_DUMMY(5),
                I1=>Q_DUMMY(4),
                I2=>T4,
                O=>T6);
   
   I_36_15 : AND4
      port map (I0=>Q_DUMMY(3),
                I1=>Q_DUMMY(2),
                I2=>Q_DUMMY(1),
                I3=>Q_DUMMY(0),
                O=>T4);
   
   I_36_16 : VCC
      port map (P=>XLXN_1);
   
   I_36_24 : AND2
      port map (I0=>Q_DUMMY(1),
                I1=>Q_DUMMY(0),
                O=>T2);
   
   I_36_26 : AND3
      port map (I0=>Q_DUMMY(2),
                I1=>Q_DUMMY(1),
                I2=>Q_DUMMY(0),
                O=>T3);
   
   I_36_28 : AND4
      port map (I0=>Q_DUMMY(6),
                I1=>Q_DUMMY(5),
                I2=>Q_DUMMY(4),
                I3=>T4,
                O=>T7);
   
   I_36_31 : AND2
      port map (I0=>CE,
                I1=>TC_DUMMY,
                O=>CEO);
   
   Q0 : FTCE_MXILINX_a2dtiming
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>XLXN_1,
                Q=>Q_DUMMY(0));
   
   Q1 : FTCE_MXILINX_a2dtiming
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>Q_DUMMY(0),
                Q=>Q_DUMMY(1));
   
   Q2 : FTCE_MXILINX_a2dtiming
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>T2,
                Q=>Q_DUMMY(2));
   
   Q3 : FTCE_MXILINX_a2dtiming
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>T3,
                Q=>Q_DUMMY(3));
   
   Q4 : FTCE_MXILINX_a2dtiming
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>T4,
                Q=>Q_DUMMY(4));
   
   Q5 : FTCE_MXILINX_a2dtiming
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>T5,
                Q=>Q_DUMMY(5));
   
   Q6 : FTCE_MXILINX_a2dtiming
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>T6,
                Q=>Q_DUMMY(6));
   
   Q7 : FTCE_MXILINX_a2dtiming
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                T=>T7,
                Q=>Q_DUMMY(7));
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity BUFE4_MXILINX_a2dtiming is
   port ( E  : in    std_logic; 
          I0 : in    std_logic; 
          I1 : in    std_logic; 
          I2 : in    std_logic; 
          I3 : in    std_logic; 
          O0 : out   std_logic; 
          O1 : out   std_logic; 
          O2 : out   std_logic; 
          O3 : out   std_logic);
end BUFE4_MXILINX_a2dtiming;

architecture BEHAVIORAL of BUFE4_MXILINX_a2dtiming is
   attribute BOX_TYPE   : string ;
   component BUFE
      port ( E : in    std_logic; 
             I : in    std_logic; 
             O : out   std_logic);
   end component;
   attribute BOX_TYPE of BUFE : component is "BLACK_BOX";
   
begin
   I_36_37 : BUFE
      port map (E=>E,
                I=>I3,
                O=>O3);
   
   I_36_38 : BUFE
      port map (E=>E,
                I=>I2,
                O=>O2);
   
   I_36_39 : BUFE
      port map (E=>E,
                I=>I1,
                O=>O1);
   
   I_36_40 : BUFE
      port map (E=>E,
                I=>I0,
                O=>O0);
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity INV4_MXILINX_a2dtiming is
   port ( I0 : in    std_logic; 
          I1 : in    std_logic; 
          I2 : in    std_logic; 
          I3 : in    std_logic; 
          O0 : out   std_logic; 
          O1 : out   std_logic; 
          O2 : out   std_logic; 
          O3 : out   std_logic);
end INV4_MXILINX_a2dtiming;

architecture BEHAVIORAL of INV4_MXILINX_a2dtiming is
   attribute BOX_TYPE   : string ;
   component INV
      port ( I : in    std_logic; 
             O : out   std_logic);
   end component;
   attribute BOX_TYPE of INV : component is "BLACK_BOX";
   
begin
   I_36_37 : INV
      port map (I=>I3,
                O=>O3);
   
   I_36_38 : INV
      port map (I=>I2,
                O=>O2);
   
   I_36_39 : INV
      port map (I=>I1,
                O=>O1);
   
   I_36_40 : INV
      port map (I=>I0,
                O=>O0);
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity D3_8E_MXILINX_a2dtiming is
   port ( A0 : in    std_logic; 
          A1 : in    std_logic; 
          A2 : in    std_logic; 
          E  : in    std_logic; 
          D0 : out   std_logic; 
          D1 : out   std_logic; 
          D2 : out   std_logic; 
          D3 : out   std_logic; 
          D4 : out   std_logic; 
          D5 : out   std_logic; 
          D6 : out   std_logic; 
          D7 : out   std_logic);
end D3_8E_MXILINX_a2dtiming;

architecture BEHAVIORAL of D3_8E_MXILINX_a2dtiming is
   attribute BOX_TYPE   : string ;
   component AND4
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             I3 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND4 : component is "BLACK_BOX";
   
   component AND4B1
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             I3 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND4B1 : component is "BLACK_BOX";
   
   component AND4B2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             I3 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND4B2 : component is "BLACK_BOX";
   
   component AND4B3
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             I3 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND4B3 : component is "BLACK_BOX";
   
begin
   I_36_30 : AND4
      port map (I0=>A2,
                I1=>A1,
                I2=>A0,
                I3=>E,
                O=>D7);
   
   I_36_31 : AND4B1
      port map (I0=>A0,
                I1=>A2,
                I2=>A1,
                I3=>E,
                O=>D6);
   
   I_36_32 : AND4B1
      port map (I0=>A1,
                I1=>A2,
                I2=>A0,
                I3=>E,
                O=>D5);
   
   I_36_33 : AND4B2
      port map (I0=>A1,
                I1=>A0,
                I2=>A2,
                I3=>E,
                O=>D4);
   
   I_36_34 : AND4B1
      port map (I0=>A2,
                I1=>A0,
                I2=>A1,
                I3=>E,
                O=>D3);
   
   I_36_35 : AND4B2
      port map (I0=>A2,
                I1=>A0,
                I2=>A1,
                I3=>E,
                O=>D2);
   
   I_36_36 : AND4B2
      port map (I0=>A2,
                I1=>A1,
                I2=>A0,
                I3=>E,
                O=>D1);
   
   I_36_37 : AND4B3
      port map (I0=>A2,
                I1=>A1,
                I2=>A0,
                I3=>E,
                O=>D0);
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity a2dtiming is
   port ( A2DINTRP : in    std_logic; 
          A2DSTAT  : in    std_logic; 
          FIFOCTL  : in    std_logic_vector (7 downto 0); 
          LBSD3    : in    std_logic; 
          ONEPPS   : in    std_logic; 
          PLLOUT   : in    std_logic; 
          SA1      : in    std_logic; 
          SA2      : in    std_logic; 
          SA3      : in    std_logic; 
          SIORW    : in    std_logic; 
          D2A0     : in    std_logic;
          A2DCLK   : out   std_logic; 
          A2DCS0N  : out   std_logic; 
          A2DCS1N  : out   std_logic; 
          A2DCS2N  : out   std_logic; 
          A2DCS3N  : out   std_logic; 
          A2DCS4N  : out   std_logic; 
          A2DCS5N  : out   std_logic; 
          A2DCS6N  : out   std_logic; 
          A2DCS7N  : out   std_logic; 
          A2DIOEBL : out   std_logic; 
          A2DRS    : out   std_logic; 
          A2DRWN   : out   std_logic; 
          A2DSYNC  : out   std_logic; 
          FIFOLDCK : out   std_logic; 
          IRQFF    : out   std_logic; 
          PRESYNC  : out   std_logic);
end a2dtiming;

architecture BEHAVIORAL of a2dtiming is
   attribute BOX_TYPE   : string ;
   attribute HU_SET     : string ;
   signal ACTR           : std_logic_vector (7 downto 0);
   signal ADCLKEN        : std_logic;
   signal A2DCS          : std_logic;
   signal PRESYNCN       : std_logic;
   signal XLXN_17        : std_logic;
   signal XLXN_18        : std_logic;
   signal XLXN_19        : std_logic;
   signal XLXN_29        : std_logic;
   signal XLXN_30        : std_logic;
   signal XLXN_31        : std_logic;
   signal XLXN_32        : std_logic;
   signal XLXN_33        : std_logic;
   signal XLXN_34        : std_logic;
   signal XLXN_35        : std_logic;
   signal XLXN_36        : std_logic;
   signal XLXN_128       : std_logic;
   signal XLXN_155       : std_logic;
   signal XLXN_223       : std_logic;
   signal XLXN_229       : std_logic;
   signal XLXN_230       : std_logic;
   signal XLXN_271       : std_logic;
   signal XLXN_317       : std_logic;
   signal XLXN_335       : std_logic;
   signal XLXN_340       : std_logic;
   signal XLXN_347       : std_logic;
   signal XLXN_364       : std_logic;
   signal XLXN_370       : std_logic;
   signal XLXN_394       : std_logic;
   signal XLXN_399       : std_logic;
   signal XLXN_401       : std_logic;
   signal XLXN_402       : std_logic;
   signal XLXN_403       : std_logic;
   signal XLXN_404       : std_logic;
   signal XLXN_405       : std_logic;
   signal XLXN_446       : std_logic;
   signal XLXN_455       : std_logic;
   signal XLXN_468       : std_logic;
   signal XLXN_469       : std_logic;
   signal XLXN_472       : std_logic;
   signal IRQFF_DUMMY    : std_logic;
   signal A2DSYNC_DUMMY  : std_logic;
   signal A2DCLK_DUMMY   : std_logic;
   signal A2DIOEBL_DUMMY : std_logic;
   signal PRESYNC_DUMMY  : std_logic;
   signal A2DRS_DUMMY    : std_logic;
--	signal A2DCS_Hold     : std_logic;
--   signal disable        : std_logic;
	
   component INV
      port ( I : in    std_logic; 
             O : out   std_logic);
   end component;
   attribute BOX_TYPE of INV : component is "BLACK_BOX";
   
   component BUFE4_MXILINX_a2dtiming
      port ( E  : in    std_logic; 
             I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             I3 : in    std_logic; 
             O0 : out   std_logic; 
             O1 : out   std_logic; 
             O2 : out   std_logic; 
             O3 : out   std_logic);
   end component;
   
   component BUFE
      port ( E : in    std_logic; 
             I : in    std_logic; 
             O : out   std_logic);
   end component;
   attribute BOX_TYPE of BUFE : component is "BLACK_BOX";
   
   component AND2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND2 : component is "BLACK_BOX";
   
   component FDC_MXILINX_a2dtiming
      port ( C   : in    std_logic; 
             CLR : in    std_logic; 
             D   : in    std_logic; 
             Q   : out   std_logic);
   end component;
   
   component OR2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of OR2 : component is "BLACK_BOX";
   
   component D3_8E_MXILINX_a2dtiming
      port ( A0 : in    std_logic; 
             A1 : in    std_logic; 
             A2 : in    std_logic; 
             E  : in    std_logic; 
             D0 : out   std_logic; 
             D1 : out   std_logic; 
             D2 : out   std_logic; 
             D3 : out   std_logic; 
             D4 : out   std_logic; 
             D5 : out   std_logic; 
             D6 : out   std_logic; 
             D7 : out   std_logic);
   end component;
   
   component INV4_MXILINX_a2dtiming
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             I3 : in    std_logic; 
             O0 : out   std_logic; 
             O1 : out   std_logic; 
             O2 : out   std_logic; 
             O3 : out   std_logic);
   end component;
   
   component NAND2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of NAND2 : component is "BLACK_BOX";
   
   component VCC
      port ( P : out   std_logic);
   end component;
   attribute BOX_TYPE of VCC : component is "BLACK_BOX";
   
   component CB8CE_MXILINX_a2dtiming
      port ( C   : in    std_logic; 
             CE  : in    std_logic; 
             CLR : in    std_logic; 
             CEO : out   std_logic; 
             Q   : out   std_logic_vector (7 downto 0); 
             TC  : out   std_logic);
   end component;
   
   component FD_MXILINX_a2dtiming
      port ( C : in    std_logic; 
             D : in    std_logic; 
             Q : out   std_logic);
   end component;
   
   component AND2B1
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND2B1 : component is "BLACK_BOX";
   
   component GND
      port ( G : out   std_logic);
   end component;
   attribute BOX_TYPE of GND : component is "BLACK_BOX";
   
   component AND3B2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             O  : out   std_logic);
   end component;

   component AND3B1
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             O  : out   std_logic);
   end component;

   attribute BOX_TYPE of AND3B2 : component is "BLACK_BOX";
   
   component NAND3B1
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             I2 : in    std_logic; 
             O  : out   std_logic);
   end component;
		
   attribute BOX_TYPE of NAND3B1 : component is "BLACK_BOX";
   
   attribute HU_SET of XLXI_6 : label is "XLXI_6_8";
   attribute HU_SET of XLXI_63 : label is "XLXI_63_9";
   attribute HU_SET of XLXI_93 : label is "XLXI_93_13";
   attribute HU_SET of XLXI_99 : label is "XLXI_99_10";
   attribute HU_SET of XLXI_100 : label is "XLXI_100_11";
   attribute HU_SET of XLXI_101 : label is "XLXI_101_12";
   attribute HU_SET of XLXI_127 : label is "XLXI_127_14";
   attribute HU_SET of XLXI_131 : label is "XLXI_131_15";
   attribute HU_SET of XLXI_138 : label is "XLXI_138_16";
   attribute HU_SET of XLXI_141 : label is "XLXI_141_17";
   attribute HU_SET of XLXI_181 : label is "XLXI_181_18";
   attribute HU_SET of XLXI_185 : label is "XLXI_185_19";
begin
   A2DCLK <= A2DCLK_DUMMY;
   A2DIOEBL <= A2DIOEBL_DUMMY;
   A2DRS <= A2DRS_DUMMY;
   A2DSYNC <= A2DSYNC_DUMMY;
   IRQFF <= IRQFF_DUMMY;
   PRESYNC <= PRESYNC_DUMMY;
--	disable <= XLXN_401 and XLXN_128;
--	A2DCS_Hold <= (XLXN_401) OR (NOT ACTR(4));
	
--   XLXI_my : GND
--      port map (G=>disable);
		
   XLXI_2 : INV
      port map (I=>ACTR(4),
                O=>XLXN_340);
   
   XLXI_6 : BUFE4_MXILINX_a2dtiming
      port map (E=>XLXN_128,
                I0=>XLXN_394,
                I1=>SA1,
                I2=>SA2,
                I3=>SA3,
                O0=>XLXN_399,
                O1=>XLXN_17,
                O2=>XLXN_18,
                O3=>XLXN_19);
   
   XLXI_7 : BUFE
      port map (E=>XLXN_128,
                I=>XLXN_223,
                O=>A2DCS);
   
   XLXI_8 : BUFE
      port map (E=>FIFOCTL(1),
                I=>XLXN_347,
                O=>A2DCS);
   
   XLXI_37 : INV
      port map (I=>FIFOCTL(1),
                O=>XLXN_128);
   
   XLXI_60 : AND2
      port map (I0=>FIFOCTL(1),
                I1=>IRQFF_DUMMY,
                O=>XLXN_155);
   
   XLXI_63 : FDC_MXILINX_a2dtiming
      port map (C=>A2DINTRP,
                CLR=>IRQFF_DUMMY,
                D=>XLXN_317,
                Q=>XLXN_335);
   
   XLXI_91 : OR2
      port map (I0=>A2DSTAT,
                I1=>D2A0,
                O=>A2DIOEBL_DUMMY);
   
   XLXI_93 : FDC_MXILINX_a2dtiming
      port map (C=>FIFOCTL(3),
                CLR=>XLXN_229,
                D=>FIFOCTL(2),
                Q=>PRESYNC_DUMMY);
   
   XLXI_94 : AND2
      port map (I0=>XLXN_230,
                I1=>FIFOCTL(4),
                O=>XLXN_229);
   
   XLXI_96 : INV
      port map (I=>ONEPPS,
                O=>XLXN_230);
   
   XLXI_98 : AND2
      port map (I0=>SIORW,
                I1=>A2DIOEBL_DUMMY,
                O=>XLXN_223);
                
   XLXI_80 : INV
      port map (I=>XLXN_401,
                O=>XLXN_402);      
   
   XLXI_81 : INV
      port map (I=>XLXN_402,
                O=>XLXN_403);      
   XLXI_82 : INV
      port map (I=>XLXN_403,
                O=>XLXN_404);      
   XLXI_83 : INV
      port map (I=>XLXN_404,
                O=>XLXN_405);      
                
   XLXI_99 : D3_8E_MXILINX_a2dtiming
      port map (A0=>XLXN_17,
                A1=>XLXN_18,
                A2=>XLXN_19,
--					 E=>disable,
--                E=>XLXN_401,
                E=>XLXN_405,
--                E=>A2DCS_Hold,
                D0=>XLXN_29,
                D1=>XLXN_30,
                D2=>XLXN_31,
                D3=>XLXN_32,
                D4=>XLXN_33,
                D5=>XLXN_34,
                D6=>XLXN_35,
                D7=>XLXN_36);
   
   XLXI_100 : INV4_MXILINX_a2dtiming
      port map (I0=>XLXN_32,
                I1=>XLXN_31,
                I2=>XLXN_30,
                I3=>XLXN_29,
                O0=>A2DCS3N,
                O1=>A2DCS2N,
                O2=>A2DCS1N,
                O3=>A2DCS0N);
   
   XLXI_101 : INV4_MXILINX_a2dtiming
      port map (I0=>XLXN_36,
                I1=>XLXN_35,
                I2=>XLXN_34,
                I3=>XLXN_33,
                O0=>A2DCS7N,
                O1=>A2DCS6N,
                O2=>A2DCS5N,
                O3=>A2DCS4N);
   
   XLXI_116 : NAND2
      port map (I0=>LBSD3,
                I1=>A2DIOEBL_DUMMY,
                O=>XLXN_271);
   
   XLXI_117 : NAND2
      port map (I0=>XLXN_271,
                I1=>XLXN_128,
                O=>A2DRWN);
   
   XLXI_119 : VCC
      port map (P=>XLXN_317);
   
   XLXI_127 : BUFE4_MXILINX_a2dtiming
      port map (E=>FIFOCTL(1),
                I0=>ACTR(0),
                I1=>ACTR(1),
                I2=>ACTR(2),
                I3=>ACTR(3),
                O0=>XLXN_399,
                O1=>XLXN_17,
                O2=>XLXN_18,
                O3=>XLXN_19);
   
   XLXI_131 : CB8CE_MXILINX_a2dtiming
      port map (C=>A2DCLK_DUMMY,
                CE=>XLXN_340,
                CLR=>XLXN_155,
                CEO=>open,
                Q(7 downto 0)=>ACTR(7 downto 0),
                TC=>open);
   
   XLXI_138 : FDC_MXILINX_a2dtiming
      port map (C=>A2DCLK_DUMMY,
                CLR=>XLXN_364,
                D=>XLXN_335,
                Q=>IRQFF_DUMMY);
   
   XLXI_139 : INV
      port map (I=>FIFOCTL(1),
                O=>XLXN_364);
   
   XLXI_141 : FD_MXILINX_a2dtiming
      port map (C=>SIORW,
                D=>XLXN_370,
                Q=>ADCLKEN);
   
   XLXI_143 : VCC
      port map (P=>XLXN_370);
   
   XLXI_144 : AND2
      port map (I0=>ADCLKEN,
                I1=>PLLOUT,
                O=>A2DCLK_DUMMY);
   
   XLXI_154 : AND2B1
      port map (I0=>XLXN_446,
                I1=>A2DCS,
                O=>XLXN_401);
   
   XLXI_155 : GND
      port map (G=>XLXN_394);
   
   XLXI_165 : BUFE
      port map (E=>XLXN_128,
                I=>A2DSTAT,
                O=>A2DRS_DUMMY);
   
   XLXI_166 : BUFE
      port map (E=>FIFOCTL(1),
                I=>XLXN_455,
                O=>A2DRS_DUMMY);
   
   XLXI_175 : AND2B1
      port map (I0=>FIFOCTL(6),
                I1=>XLXN_399,
--                  I1=>A2DCS,
              O=>XLXN_446);
   
   XLXI_177 : AND2
      port map (I0=>FIFOCTL(6),
                I1=>XLXN_468,
                O=>XLXN_455);
   
   XLXI_180 : INV
      port map (I=>ACTR(0),
                O=>XLXN_468);
   
   XLXI_181 : FDC_MXILINX_a2dtiming
      port map (C=>PRESYNCN,
                CLR=>A2DSYNC_DUMMY,
                D=>XLXN_469,
                Q=>XLXN_472);
   
   XLXI_183 : VCC
      port map (P=>XLXN_469);
   
   XLXI_184 : INV
      port map (I=>PRESYNC_DUMMY,
                O=>PRESYNCN);
   
   XLXI_185 : FD_MXILINX_a2dtiming
      port map (C=>A2DCLK_DUMMY,
                D=>XLXN_472,
                Q=>A2DSYNC_DUMMY);
   
   XLXI_187 : AND3B2
      port map (I0=>A2DCLK_DUMMY,
                I1=>IRQFF_DUMMY,
                I2=>XLXN_340,
                O=>XLXN_347);
   
   XLXI_188 : NAND3B1
      port map (I0=>XLXN_446,
                I1=>A2DCS,
                I2=>PRESYNCN,
                O=>FIFOLDCK);
   
end BEHAVIORAL;



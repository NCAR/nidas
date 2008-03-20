--------------------------------------------------------------------------------
-- Copyright (c) 1995-2003 Xilinx, Inc.
-- All Right Reserved.
--------------------------------------------------------------------------------
--   ____  ____ 
--  /   /\/   / 
-- /___/  \  /    Vendor: Xilinx 
-- \   \   \/     Version : 7.1.04i
--  \   \         Application : sch2vhdl
--  /   /         Filename : fifoio.vhf
-- /___/   /\     Timestamp : 02/10/2006 14:47:24
-- \   \  /  \ 
--  \___\/\___\ 
--
--Command: C:/Xilinx/bin/nt/sch2vhdl.exe -intstyle ise -family xc9500 -flat -suppress -w fifoio.sch fifoio.vhf
--Design Name: fifoio
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

entity FD_MXILINX_fifoio is
   port ( C : in    std_logic; 
          D : in    std_logic; 
          Q : out   std_logic);
end FD_MXILINX_fifoio;

architecture BEHAVIORAL of FD_MXILINX_fifoio is
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

entity BUFE4_MXILINX_fifoio is
   port ( E  : in    std_logic; 
          I0 : in    std_logic; 
          I1 : in    std_logic; 
          I2 : in    std_logic; 
          I3 : in    std_logic; 
          O0 : out   std_logic; 
          O1 : out   std_logic; 
          O2 : out   std_logic; 
          O3 : out   std_logic);
end BUFE4_MXILINX_fifoio;

architecture BEHAVIORAL of BUFE4_MXILINX_fifoio is
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

entity FD8CE_MXILINX_fifoio is
   port ( C   : in    std_logic; 
          CE  : in    std_logic; 
          CLR : in    std_logic; 
          D   : in    std_logic_vector (7 downto 0); 
          Q   : out   std_logic_vector (7 downto 0));
end FD8CE_MXILINX_fifoio;

architecture BEHAVIORAL of FD8CE_MXILINX_fifoio is
   attribute BOX_TYPE   : string ;
   component FDCE
      port ( C   : in    std_logic; 
             CE  : in    std_logic; 
             CLR : in    std_logic; 
             D   : in    std_logic; 
             Q   : out   std_logic);
   end component;
   attribute BOX_TYPE of FDCE : component is "BLACK_BOX";
   
begin
   Q0 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(0),
                Q=>Q(0));
   
   Q1 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(1),
                Q=>Q(1));
   
   Q2 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(2),
                Q=>Q(2));
   
   Q3 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(3),
                Q=>Q(3));
   
   Q4 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(4),
                Q=>Q(4));
   
   Q5 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(5),
                Q=>Q(5));
   
   Q6 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(6),
                Q=>Q(6));
   
   Q7 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(7),
                Q=>Q(7));
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity fifoio is
   port ( A2DSYNC    : in    std_logic; 
          FIFO       : in    std_logic; 
          FIFOAFAE   : in    std_logic; 
          FIFOEMPTYN : in    std_logic; 
          FIFOFULL   : in    std_logic; 
          FIFOHF     : in    std_logic; 
          FIFOSTAT   : in    std_logic; 
          ONEPPS     : in    std_logic; 
          PRESYNC    : in    std_logic; 
          SIOR       : in    std_logic; 
          SIOWN      : in    std_logic; 
			    clk        : in    std_logic;
          FIFOCLRN   : out   std_logic; 
          FIFOCTL    : out   std_logic_vector (7 downto 0); 
          FIFODAFN   : out   std_logic; 
          FIFOOE     : out   std_logic; 
          FIFOUNCK   : out   std_logic; 
--          TEST43     : out   std_logic; 
          BSD        : inout std_logic_vector (15 downto 0));
end fifoio;

architecture BEHAVIORAL of fifoio is
   attribute HU_SET     : string ;
   attribute BOX_TYPE   : string ;
	
   signal LLO           : std_logic;
   signal LL0           : std_logic;
   signal LL1           : std_logic;
   signal XLXN_10       : std_logic;
   signal XLXN_30       : std_logic;
   signal FIFOCTL_DUMMY : std_logic_vector (7 downto 0);
   signal FIFOOE_DUMMY  : std_logic;
   signal FIFOOE_DELAY  : std_logic;
   signal PLL_DIV    : std_logic;
	
   component FD8CE_MXILINX_fifoio
      port ( C   : in    std_logic; 
             CE  : in    std_logic; 
             CLR : in    std_logic; 
             D   : in    std_logic_vector (7 downto 0); 
             Q   : out   std_logic_vector (7 downto 0));
   end component;
   
   component GND
      port ( G : out   std_logic);
   end component;
   attribute BOX_TYPE of GND : component is "BLACK_BOX";
   
   component INV
      port ( I : in    std_logic; 
             O : out   std_logic);
   end component;
   attribute BOX_TYPE of INV : component is "BLACK_BOX";
   
   component BUFE4_MXILINX_fifoio
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
   
   component BUF
      port ( I : in    std_logic; 
             O : out   std_logic);
   end component;
   attribute BOX_TYPE of BUF : component is "BLACK_BOX";
   
   component NOR2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
	
   component FD_MXILINX_fifoio
    port ( C : in    std_logic; 
           D : in    std_logic; 
           Q : out   std_logic);
   end component;

   component BUFE
      port ( E : in    std_logic; 
             I : in    std_logic; 
             O : out   std_logic);
   end component;

   attribute BOX_TYPE of NOR2 : component is "BLACK_BOX";
   attribute BOX_TYPE of BUFE : component is "BLACK_BOX";   
   attribute HU_SET of XLXI_4 : label is "XLXI_4_1";
   attribute HU_SET of XLXI_19 : label is "XLXI_19_0";
   attribute HU_SET of XLXI_29 : label is "XLXI_29_2";
   attribute HU_SET of XLXI_37 : label is "XLXI_37_3";
   attribute HU_SET of XLXI_38 : label is "XLXI_38_4";
begin
   FIFOCTL(7 downto 0) <= FIFOCTL_DUMMY(7 downto 0);
   FIFOOE <= FIFOOE_DUMMY;
   XLXI_4 : FD8CE_MXILINX_fifoio
      port map (C=>SIOWN,
                CE=>FIFO,
                CLR=>XLXN_10,
                D(7 downto 0)=>BSD(7 downto 0),
                Q(7 downto 0)=>FIFOCTL_DUMMY(7 downto 0));
   
   XLXI_5 : GND
      port map (G=>XLXN_10);
   
   XLXI_17 : INV
      port map (I=>FIFOCTL_DUMMY(5),
                O=>FIFODAFN);
   
   XLXI_19 : BUFE4_MXILINX_fifoio
      port map (E=>XLXN_30,
                I0=>FIFOHF,
                I1=>FIFOAFAE,
                I2=>FIFOEMPTYN,
                I3=>FIFOFULL,
                O0=>BSD(0),
                O1=>BSD(1),
                O2=>BSD(2),
                O3=>BSD(3));
   
   XLXI_20 : AND2
      port map (I0=>FIFOSTAT,
                I1=>SIOR,
                O=>XLXN_30);
   
   XLXI_23 : AND2
      port map (I0=>SIOR,
                I1=>FIFO,
                O=>FIFOOE_DELAY);
   
   XLXI_28 : INV
      port map (I=>FIFOOE_DUMMY,
                O=>FIFOUNCK);
   
   XLXI_29 : BUFE4_MXILINX_fifoio
      port map (E=>XLXN_30,
                I0=>ONEPPS,
                I1=>PRESYNC,
                I2=>LL0,
                I3=>LL0,
                O0=>BSD(4),
                O1=>BSD(5),
                O2=>BSD(6),
                O3=>BSD(7));
   
--   XLXI_31 : BUFE4_MXILINX_fifoio
--      port map (E=>XLXN_30,
--                I0=>A2DINT(1),
--                I1=>A2DINT(2),
--                I2=>A2DINT(3),
--                I3=>A2DINT(4),
--                O0=>BSD(8),
--                O1=>BSD(9),
--                O2=>BSD(10),
--                O3=>BSD(11));
   
--   XLXI_32 : BUFE4_MXILINX_fifoio
--      port map (E=>XLXN_30,
--                I0=>A2DINT(4),
--                I1=>A2DINT(5),
--                I2=>A2DINT(6),
--                I3=>A2DINT(7),
--                O0=>BSD(12),
--                O1=>BSD(13),
--                O2=>BSD(14),
--                O3=>BSD(15));
   
   XLXI_33 : GND
      port map (G=>LLO);
   
   XLXI_34 : VCC
      port map (P=>LL1);
   
   
   XLXI_36 : NOR2
      port map (I0=>A2DSYNC,
                I1=>FIFOCTL_DUMMY(0),
                O=>FIFOCLRN);
					 
   XLXI_37 : FD_MXILINX_fifoio
      port map (C=>clk,
                D=>FIFOOE_DELAY,
                Q=>PLL_DIV);
					 
   XLXI_38 : FD_MXILINX_fifoio
      port map (C=>clk,
                D=>PLL_DIV,
                Q=>FIFOOE_DUMMY);
                         
end BEHAVIORAL;



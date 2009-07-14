--------------------------------------------------------------------------------
-- Copyright (c) 1995-2003 Xilinx, Inc.
-- All Right Reserved.
--------------------------------------------------------------------------------
--   ____  ____ 
--  /   /\/   / 
-- /___/  \  /    Vendor: Xilinx 
-- \   \   \/     Version : 7.1.04i
--  \   \         Application : sch2vhdl
--  /   /         Filename : ISAintfc.vhf
-- /___/   /\     Timestamp : 01/05/2006 10:51:23
-- \   \  /  \ 
--  \___\/\___\ 
--
--Command: C:/Xilinx/bin/nt/sch2vhdl.exe -intstyle ise -family xc9500 -flat -suppress -w ISAintfc.sch ISAintfc.vhf
--Design Name: ISAintfc
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

entity FD_MXILINX_ISAintfc is
   port ( C : in    std_logic; 
          D : in    std_logic; 
          Q : out   std_logic);
end FD_MXILINX_ISAintfc;

architecture BEHAVIORAL of FD_MXILINX_ISAintfc is
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

entity D3_8E_MXILINX_ISAintfc is
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
end D3_8E_MXILINX_ISAintfc;

architecture BEHAVIORAL of D3_8E_MXILINX_ISAintfc is
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

entity FD4CE_MXILINX_ISAintfc is
   port ( C   : in    std_logic; 
          CE  : in    std_logic; 
          CLR : in    std_logic; 
          D0  : in    std_logic; 
          D1  : in    std_logic; 
          D2  : in    std_logic; 
          D3  : in    std_logic; 
          Q0  : out   std_logic; 
          Q1  : out   std_logic; 
          Q2  : out   std_logic; 
          Q3  : out   std_logic);
end FD4CE_MXILINX_ISAintfc;

architecture BEHAVIORAL of FD4CE_MXILINX_ISAintfc is
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
   U0 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D0,
                Q=>Q0);
   
   U1 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D1,
                Q=>Q1);
   
   U2 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D2,
                Q=>Q2);
   
   U3 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D3,
                Q=>Q3);
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity ISAintfc is
   port ( BRDSELO  : in    std_logic; 
          FIFOCTL  : in    std_logic_vector (7 downto 0); 
          IORN     : in    std_logic; 
          IOWN     : in    std_logic; 
          SA0      : in    std_logic; 
          SA1      : in    std_logic; 
          SA2      : in    std_logic; 
          SA3      : in    std_logic; 
          SA4      : in    std_logic; 
		    PLLOUT   : in    std_logic;
          A2DDATA  : out   std_logic; 
          A2DSTAT  : out   std_logic; 
          D2A0     : out   std_logic; 
          D2A1     : out   std_logic; 
          D2A2     : out   std_logic; 
          FIFO     : out   std_logic; 
          FIFOSTAT : out   std_logic; 
          IOCS16N  : out   std_logic; 
          I2CSCL   : out   std_logic; 
          LBSD3    : out   std_logic; 
          SIOR     : out   std_logic; 
          SIORN    : out   std_logic; 
          SIORW    : out   std_logic; 
          SIOW     : out   std_logic; 
          SIOWN    : out   std_logic; 
          SYSCTL   : out   std_logic; 
          BSD      : inout std_logic_vector (15 downto 0); 
          I2CSDA   : inout std_logic);
end ISAintfc;

architecture BEHAVIORAL of ISAintfc is
   attribute BOX_TYPE   : string ;
   attribute HU_SET     : string ;
   signal BRDSELI       : std_logic;
   signal FSEL          : std_logic;
   signal XLXN_157      : std_logic;
   signal XLXN_158      : std_logic;
   signal XLXN_159      : std_logic;
   signal XLXN_161      : std_logic;
   signal XLXN_206      : std_logic;
   signal XLXN_210      : std_logic;
   signal XLXN_212      : std_logic;
   signal XLXN_214      : std_logic;
   signal XLXN_216      : std_logic;
   signal XLXN_221      : std_logic;
   signal XLXN_222      : std_logic;
   signal XLXN_370      : std_logic;
--   signal XLXN_223a      : std_logic;
--   signal XLXN_223b      : std_logic;
--   signal XLXN_223c      : std_logic;
--   signal XLXN_223d      : std_logic;
--   signal XLXN_223e      : std_logic;
--   signal XLXN_223f      : std_logic;
   signal XLXN_225      : std_logic;
   signal XLXN_229      : std_logic;
   signal XLXN_230      : std_logic;
   signal XLXN_231      : std_logic;
   signal XLXN_232      : std_logic;
   signal A2DSTAT_DUMMY : std_logic;
   signal A2DDATA_DUMMY : std_logic;
   signal SIOWN_DUMMY   : std_logic;
   signal SIOR_DUMMY    : std_logic;
   signal SIORN_DUMMY   : std_logic;
   signal SIOW_DUMMY    : std_logic;
   signal SIORW_DUMMY   : std_logic;
	signal D2A2_DUMMY    : std_logic;
--   signal MY_SIORW      : std_logic;
--	signal cnt           : std_logic_vector(1 downto 0):="00";
   component INV
      port ( I : in    std_logic; 
             O : out   std_logic);
   end component;
   attribute BOX_TYPE of INV : component is "BLACK_BOX";
   
   component FD4CE_MXILINX_ISAintfc
      port ( C   : in    std_logic; 
             CE  : in    std_logic; 
             CLR : in    std_logic; 
             D0  : in    std_logic; 
             D1  : in    std_logic; 
             D2  : in    std_logic; 
             D3  : in    std_logic; 
             Q0  : out   std_logic; 
             Q1  : out   std_logic; 
             Q2  : out   std_logic; 
             Q3  : out   std_logic);
   end component;
   
   component D3_8E_MXILINX_ISAintfc
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
   
   component GND
      port ( G : out   std_logic);
   end component;
   attribute BOX_TYPE of GND : component is "BLACK_BOX";

   component VCC
      port ( P : out   std_logic);
   end component;
   attribute BOX_TYPE of VCC : component is "BLACK_BOX";
  
   component NAND2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of NAND2 : component is "BLACK_BOX";
   
   component NOR2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of NOR2 : component is "BLACK_BOX";
   
   component BUFE
      port ( E : in    std_logic; 
             I : in    std_logic; 
             O : out   std_logic);
   end component;
   attribute BOX_TYPE of BUFE : component is "BLACK_BOX";
   
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
   
   component FD_MXILINX_ISAintfc
      port ( C : in    std_logic; 
             D : in    std_logic; 
             Q : out   std_logic);
   end component;
   
   attribute HU_SET of XLXI_60 : label is "XLXI_60_0";
   attribute HU_SET of XLXI_61 : label is "XLXI_61_1";
   attribute HU_SET of XLXI_102 : label is "XLXI_102_2";
   attribute HU_SET of XLXI_103 : label is "XLXI_103_3";
begin
   A2DSTAT <= A2DSTAT_DUMMY;
   A2DDATA <= A2DDATA_DUMMY;
   SIOR <= SIOR_DUMMY;
   SIORN <= SIORN_DUMMY;
   SIOW <= SIOW_DUMMY;
   SIOWN <= SIOWN_DUMMY;
   D2A2 <= D2A2_DUMMY;

   XLXI_30 : INV
      port map (I=>SIOW_DUMMY,
                O=>SIOWN_DUMMY); 
                
   XLXI_143 : VCC
      port map (P=>XLXN_370);                      
   
   XLXI_60 : FD4CE_MXILINX_ISAintfc
      port map (C=>SIOWN_DUMMY,
                CE=>FSEL,
                CLR=>XLXN_210,
                D0=>BSD(0),
                D1=>BSD(1),
                D2=>BSD(2),
                D3=>BSD(3),
                Q0=>XLXN_157,
                Q1=>XLXN_158,
                Q2=>XLXN_159,
                Q3=>LBSD3);
   
   XLXI_61 : D3_8E_MXILINX_ISAintfc
      port map (A0=>XLXN_157,
                A1=>XLXN_158,
                A2=>XLXN_159,
                E=>XLXN_206,
                D0=>FIFO,
                D1=>A2DSTAT_DUMMY,
                D2=>A2DDATA_DUMMY,
                D3=>D2A0,
                D4=>D2A1,
                D5=>D2A2_DUMMY,
                D6=>SYSCTL,
                D7=>FIFOSTAT);
   
   XLXI_63 : GND
      port map (G=>XLXN_210);
        
   XLXI_66 : NAND2
      port map (I0=>SIORN_DUMMY,
                I1=>SIOWN_DUMMY,
                O=>SIORW);
   
   XLXI_69 : INV
      port map (I=>SIOR_DUMMY,
                O=>SIORN_DUMMY);
   
   XLXI_75 : NOR2
      port map (I0=>IOWN,
                I1=>BRDSELO,
                O=>SIOW_DUMMY);
   
   XLXI_76 : NOR2
      port map (I0=>IORN,
                I1=>BRDSELO,
                O=>SIOR_DUMMY);
   
   XLXI_80 : BUFE
      port map (E=>BRDSELI,
                I=>XLXN_161,
                O=>IOCS16N);
   
   XLXI_81 : INV
      port map (I=>BRDSELO,
                O=>BRDSELI);
   
   XLXI_82 : GND
      port map (G=>XLXN_161);   
   
   XLXI_89 : INV
      port map (I=>XLXN_229,
                O=>XLXN_232);
                
   XLXI_90 : INV
      port map (I=>FSEL,
                O=>XLXN_206);
                
   XLXI_91 : AND2
      port map (I0=>XLXN_232,
                I1=>XLXN_221,
                O=>XLXN_231);
   
   XLXI_92 : AND5
      port map (I0=>BRDSELI,
                I1=>SA3,
                I2=>SA2,
                I3=>SA1,
                I4=>SA0,   --Uncomment for Viper
--                I4=>SA4, --Uncomment for Vulcan
                O=>FSEL);
   
   XLXI_93 : AND2
--      port map (I0=>FIFOCTL(1),
--                I1=>A2DSTAT_DUMMY,
--      port map (I0=>FIFOCTL(7),
--                I1=>D2A2_DUMMY,
      port map (I0=>A2DDATA_DUMMY,
                I1=>A2DDATA_DUMMY,
                O=>XLXN_230);
   
   XLXI_94 : AND2
      port map (I0=>SIOR_DUMMY,
                I1=>XLXN_230,
                O=>XLXN_229);
   
   XLXI_95 : NAND2
      port map (I0=>SIOW_DUMMY,
                I1=>XLXN_230,
                O=>XLXN_216);
   
   XLXI_96 : INV
      port map (I=>BSD(0),
                O=>XLXN_212);
   
   XLXI_97 : INV
      port map (I=>BSD(1),
                O=>XLXN_214);
   
   XLXI_98 : INV
      port map (I=>XLXN_221,
                O=>XLXN_222);
   
   XLXI_99 : INV
      port map (I=>XLXN_225,
                O=>I2CSCL);
   
   XLXI_100 : BUFE
      port map (E=>XLXN_229,
                I=>I2CSDA,
                O=>BSD(0));
   
   XLXI_101 : BUFE
      port map (E=>XLXN_231,
                I=>XLXN_222,
                O=>I2CSDA);
   
   XLXI_102 : FD_MXILINX_ISAintfc
      port map (C=>XLXN_216,
                D=>XLXN_212,
                Q=>XLXN_221);
   
   XLXI_103 : FD_MXILINX_ISAintfc
      port map (C=>XLXN_216,
                D=>XLXN_214,
                Q=>XLXN_225);
--	delay: process(pllout,FSEL)
--	begin
--	  if FSEL = '1' then
--	    cnt <= "00";
--     elsif cnt = "00" and FSEL = '0' then
--	   cnt <= "01";
--     elsif cnt = "01" and FSEL = '0' then
--	   cnt <= "10";
--	  else
--	    cnt <= cnt;
--     end if;
--	end process delay;
   
end BEHAVIORAL;



--------------------------------------------------------------------------------
-- Copyright (c) 1995-2003 Xilinx, Inc.
-- All Right Reserved.
--------------------------------------------------------------------------------
--   ____  ____ 
--  /   /\/   / 
-- /___/  \  /    Vendor: Xilinx 
-- \   \   \/     Version : 7.1.04i
--  \   \         Application : sch2vhdl
--  /   /         Filename : sysctrl.vhf
-- /___/   /\     Timestamp : 02/10/2006 14:47:26
-- \   \  /  \ 
--  \___\/\___\ 
--
--Command: C:/Xilinx/bin/nt/sch2vhdl.exe -intstyle ise -family xc9500 -flat -suppress -w sysctrl.sch sysctrl.vhf
--Design Name: sysctrl
--Device: xc9500
--Purpose:
--    This vhdl netlist is translated from an ECS schematic. It can be 
--    synthesis and simulted, but it should not be modified. 
--

--library ieee;
--use ieee.std_logic_1164.ALL;
--use ieee.numeric_std.ALL;
---- synopsys translate_off
--library UNISIM;
--use UNISIM.Vcomponents.ALL;
---- synopsys translate_on
--
--entity FD4CE_MXILINX_sysctrl is
--   port ( C   : in    std_logic; 
--          CE  : in    std_logic; 
--          CLR : in    std_logic; 
--          D0  : in    std_logic; 
--          D1  : in    std_logic; 
--          D2  : in    std_logic; 
--          D3  : in    std_logic; 
--          Q0  : out   std_logic; 
--          Q1  : out   std_logic; 
--          Q2  : out   std_logic; 
--          Q3  : out   std_logic);
--end FD4CE_MXILINX_sysctrl;
--
--architecture BEHAVIORAL of FD4CE_MXILINX_sysctrl is
--   attribute BOX_TYPE   : string ;
--   component FDCE
--      port ( C   : in    std_logic; 
--             CE  : in    std_logic; 
--             CLR : in    std_logic; 
--             D   : in    std_logic; 
--             Q   : out   std_logic);
--   end component;
--   attribute BOX_TYPE of FDCE : component is "BLACK_BOX";
--   
--begin
--   U0 : FDCE
--      port map (C=>C,
--                CE=>CE,
--                CLR=>CLR,
--                D=>D0,
--                Q=>Q0);
--   
--   U1 : FDCE
--      port map (C=>C,
--                CE=>CE,
--                CLR=>CLR,
--                D=>D1,
--                Q=>Q1);
--   
--   U2 : FDCE
--      port map (C=>C,
--                CE=>CE,
--                CLR=>CLR,
--                D=>D2,
--                Q=>Q2);
--   
--   U3 : FDCE
--      port map (C=>C,
--                CE=>CE,
--                CLR=>CLR,
--                D=>D3,
--                Q=>Q3);
--   
--end BEHAVIORAL;



--library ieee;
--use ieee.std_logic_1164.ALL;
--use ieee.numeric_std.ALL;
---- synopsys translate_off
--library UNISIM;
--use UNISIM.Vcomponents.ALL;
---- synopsys translate_on
--
--entity M2_1E_MXILINX_sysctrl is
--   port ( D0 : in    std_logic; 
--          D1 : in    std_logic; 
--          E  : in    std_logic; 
--          S0 : in    std_logic; 
--          O  : out   std_logic);
--end M2_1E_MXILINX_sysctrl;
--
--architecture BEHAVIORAL of M2_1E_MXILINX_sysctrl is
--   attribute BOX_TYPE   : string ;
--   signal M0 : std_logic;
--   signal M1 : std_logic;
--   component AND3
--      port ( I0 : in    std_logic; 
--             I1 : in    std_logic; 
--             I2 : in    std_logic; 
--             O  : out   std_logic);
--   end component;
--   attribute BOX_TYPE of AND3 : component is "BLACK_BOX";
--   
--   component AND3B1
--      port ( I0 : in    std_logic; 
--             I1 : in    std_logic; 
--             I2 : in    std_logic; 
--             O  : out   std_logic);
--   end component;
--   attribute BOX_TYPE of AND3B1 : component is "BLACK_BOX";
--   
--   component OR2
--      port ( I0 : in    std_logic; 
--             I1 : in    std_logic; 
--             O  : out   std_logic);
--   end component;
--   attribute BOX_TYPE of OR2 : component is "BLACK_BOX";
--   
--begin
--   I_36_30 : AND3
--      port map (I0=>D1,
--                I1=>E,
--                I2=>S0,
--                O=>M1);
--   
--   I_36_31 : AND3B1
--      port map (I0=>S0,
--                I1=>E,
--                I2=>D0,
--                O=>M0);
--   
--   I_36_38 : OR2
--      port map (I0=>M1,
--                I1=>M0,
--                O=>O);
--   
--end BEHAVIORAL;
--
--
--
--library ieee;
--use ieee.std_logic_1164.ALL;
--use ieee.numeric_std.ALL;
---- synopsys translate_off
--library UNISIM;
--use UNISIM.Vcomponents.ALL;
---- synopsys translate_on
--
--entity M2_1_MXILINX_sysctrl is
--   port ( D0 : in    std_logic; 
--          D1 : in    std_logic; 
--          S0 : in    std_logic; 
--          O  : out   std_logic);
--end M2_1_MXILINX_sysctrl;
--
--architecture BEHAVIORAL of M2_1_MXILINX_sysctrl is
--   attribute BOX_TYPE   : string ;
--   signal M0 : std_logic;
--   signal M1 : std_logic;
--   component AND2B1
--      port ( I0 : in    std_logic; 
--             I1 : in    std_logic; 
--             O  : out   std_logic);
--   end component;
--   attribute BOX_TYPE of AND2B1 : component is "BLACK_BOX";
--   
--   component OR2
--      port ( I0 : in    std_logic; 
--             I1 : in    std_logic; 
--             O  : out   std_logic);
--   end component;
--   attribute BOX_TYPE of OR2 : component is "BLACK_BOX";
--   
--   component AND2
--      port ( I0 : in    std_logic; 
--             I1 : in    std_logic; 
--             O  : out   std_logic);
--   end component;
--   attribute BOX_TYPE of AND2 : component is "BLACK_BOX";
--   
--begin
--   I_36_7 : AND2B1
--      port map (I0=>S0,
--                I1=>D0,
--                O=>M0);
--   
--   I_36_8 : OR2
--      port map (I0=>M1,
--                I1=>M0,
--                O=>O);
--   
--   I_36_9 : AND2
--      port map (I0=>D1,
--                I1=>S0,
--                O=>M1);
--   
--end BEHAVIORAL;
--
--
--
--library ieee;
--use ieee.std_logic_1164.ALL;
--use ieee.numeric_std.ALL;
---- synopsys translate_off
--library UNISIM;
--use UNISIM.Vcomponents.ALL;
---- synopsys translate_on
--
--entity M8_1E_MXILINX_sysctrl is
--   port ( D0 : in    std_logic; 
--          D1 : in    std_logic; 
--          D2 : in    std_logic; 
--          D3 : in    std_logic; 
--          D4 : in    std_logic; 
--          D5 : in    std_logic; 
--          D6 : in    std_logic; 
--          D7 : in    std_logic; 
--          E  : in    std_logic; 
--          S0 : in    std_logic; 
--          S1 : in    std_logic; 
--          S2 : in    std_logic; 
--          O  : out   std_logic);
--end M8_1E_MXILINX_sysctrl;
--
--architecture BEHAVIORAL of M8_1E_MXILINX_sysctrl is
--   attribute HU_SET     : string ;
--   signal M01 : std_logic;
--   signal M03 : std_logic;
--   signal M23 : std_logic;
--   signal M45 : std_logic;
--   signal M47 : std_logic;
--   signal M67 : std_logic;
--   component M2_1E_MXILINX_sysctrl
--      port ( D0 : in    std_logic; 
--             D1 : in    std_logic; 
--             E  : in    std_logic; 
--             S0 : in    std_logic; 
--             O  : out   std_logic);
--   end component;
--   
--   component M2_1_MXILINX_sysctrl
--      port ( D0 : in    std_logic; 
--             D1 : in    std_logic; 
--             S0 : in    std_logic; 
--             O  : out   std_logic);
--   end component;
--   
--   attribute HU_SET of U1 : label is "U1_6";
--   attribute HU_SET of U2 : label is "U2_5";
--   attribute HU_SET of U3 : label is "U3_4";
--   attribute HU_SET of U4 : label is "U4_3";
--   attribute HU_SET of U5 : label is "U5_1";
--   attribute HU_SET of U6 : label is "U6_0";
--   attribute HU_SET of U7 : label is "U7_2";
--begin
--   U1 : M2_1E_MXILINX_sysctrl
--      port map (D0=>D0,
--                D1=>D1,
--                E=>E,
--                S0=>S0,
--                O=>M01);
--   
--   U2 : M2_1E_MXILINX_sysctrl
--      port map (D0=>D2,
--                D1=>D3,
--                E=>E,
--                S0=>S0,
--                O=>M23);
--   
--   U3 : M2_1E_MXILINX_sysctrl
--      port map (D0=>D4,
--                D1=>D5,
--                E=>E,
--                S0=>S0,
--                O=>M45);
--   
--   U4 : M2_1E_MXILINX_sysctrl
--      port map (D0=>D6,
--                D1=>D7,
--                E=>E,
--                S0=>S0,
--                O=>M67);
--   
--   U5 : M2_1_MXILINX_sysctrl
--      port map (D0=>M01,
--                D1=>M23,
--                S0=>S1,
--                O=>M03);
--   
--   U6 : M2_1_MXILINX_sysctrl
--      port map (D0=>M45,
--                D1=>M67,
--                S0=>S1,
--                O=>M47);
--   
--   U7 : M2_1_MXILINX_sysctrl
--      port map (D0=>M03,
--                D1=>M47,
--                S0=>S2,
--                O=>O);
--   
--end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity BUFE8_MXILINX_sysctrl is
   port ( E : in    std_logic; 
          I : in    std_logic_vector (7 downto 0); 
          O : out   std_logic_vector (7 downto 0));
end BUFE8_MXILINX_sysctrl;

architecture BEHAVIORAL of BUFE8_MXILINX_sysctrl is
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
                I=>I(0),
                O=>O(0));
   
   I_36_31 : BUFE
      port map (E=>E,
                I=>I(1),
                O=>O(1));
   
   I_36_32 : BUFE
      port map (E=>E,
                I=>I(2),
                O=>O(2));
   
   I_36_33 : BUFE
      port map (E=>E,
                I=>I(3),
                O=>O(3));
   
   I_36_34 : BUFE
      port map (E=>E,
                I=>I(7),
                O=>O(7));
   
   I_36_35 : BUFE
      port map (E=>E,
                I=>I(6),
                O=>O(6));
   
   I_36_36 : BUFE
      port map (E=>E,
                I=>I(5),
                O=>O(5));
   
   I_36_37 : BUFE
      port map (E=>E,
                I=>I(4),
                O=>O(4));
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity FD16CE_MXILINX_sysctrl is
   port ( C   : in    std_logic; 
          CE  : in    std_logic; 
          CLR : in    std_logic; 
          D   : in    std_logic_vector (15 downto 0); 
          Q   : out   std_logic_vector (15 downto 0));
end FD16CE_MXILINX_sysctrl;

architecture BEHAVIORAL of FD16CE_MXILINX_sysctrl is
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
   
   Q8 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(8),
                Q=>Q(8));
   
   Q9 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(9),
                Q=>Q(9));
   
   Q10 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(10),
                Q=>Q(10));
   
   Q11 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(11),
                Q=>Q(11));
   
   Q12 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(12),
                Q=>Q(12));
   
   Q13 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(13),
                Q=>Q(13));
   
   Q14 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(14),
                Q=>Q(14));
   
   Q15 : FDCE
      port map (C=>C,
                CE=>CE,
                CLR=>CLR,
                D=>D(15),
                Q=>Q(15));
   
end BEHAVIORAL;



library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
-- synopsys translate_off
library UNISIM;
use UNISIM.Vcomponents.ALL;
-- synopsys translate_on

entity sysctrl is
   port ( A2DINT     : in    std_logic_vector (7 downto 0);
          FIFOSTAT   : in    std_logic;	
          SIOR       : in    std_logic; 
          SIOWN      : in    std_logic; 
          SYSCTL     : in    std_logic; 
          A2DINTRP   : out   std_logic; 
          CAL_OFFSET : out   std_logic_vector (15 downto 0); 
          BSD        : inout std_logic_vector (15 downto 0));
end sysctrl;

architecture BEHAVIORAL of sysctrl is
   attribute HU_SET     : string ;
   attribute BOX_TYPE   : string ;
   signal XLXN_21    : std_logic;
   signal XLXN_23    : std_logic;
   signal XLXN_40    : std_logic;
   signal XLXN_41    : std_logic;
   signal XLXN_42    : std_logic;
   signal XLXN_43    : std_logic;
   signal XLXN_52    : std_logic;
   signal calnot     : std_logic_vector(15 downto 0):=X"FFFF";
   
   component FD16CE_MXILINX_sysctrl
      port ( C   : in    std_logic; 
             CE  : in    std_logic; 
             CLR : in    std_logic; 
             D   : in    std_logic_vector (15 downto 0); 
             Q   : out   std_logic_vector (15 downto 0));
   end component;
   
   component BUFE8_MXILINX_sysctrl
      port ( E : in    std_logic; 
             I : in    std_logic_vector (7 downto 0); 
             O : out   std_logic_vector (7 downto 0));
   end component;
   
   component AND2
      port ( I0 : in    std_logic; 
             I1 : in    std_logic; 
             O  : out   std_logic);
   end component;
   attribute BOX_TYPE of AND2 : component is "BLACK_BOX";
   
   component GND
      port ( G : out   std_logic);
   end component;
   attribute BOX_TYPE of GND : component is "BLACK_BOX";
   
--   component M8_1E_MXILINX_sysctrl
--      port ( D0 : in    std_logic; 
--             D1 : in    std_logic; 
--             D2 : in    std_logic; 
--             D3 : in    std_logic; 
--             D4 : in    std_logic; 
--             D5 : in    std_logic; 
--             D6 : in    std_logic; 
--             D7 : in    std_logic; 
--             E  : in    std_logic; 
--             S0 : in    std_logic; 
--             S1 : in    std_logic; 
--             S2 : in    std_logic; 
--             O  : out   std_logic);
--   end component;
--   
--   component FD4CE_MXILINX_sysctrl
--      port ( C   : in    std_logic; 
--             CE  : in    std_logic; 
--             CLR : in    std_logic; 
--             D0  : in    std_logic; 
--             D1  : in    std_logic; 
--             D2  : in    std_logic; 
--             D3  : in    std_logic; 
--             Q0  : out   std_logic; 
--             Q1  : out   std_logic; 
--             Q2  : out   std_logic; 
--             Q3  : out   std_logic);
--   end component;
   
   component VCC
      port ( P : out   std_logic);
   end component;
   attribute BOX_TYPE of VCC : component is "BLACK_BOX";
   
   attribute HU_SET of XLXI_1 : label is "XLXI_1_7";
   attribute HU_SET of XLXI_3 : label is "XLXI_3_8";
--   attribute HU_SET of XLXI_15 : label is "XLXI_15_9";
--   attribute HU_SET of XLXI_16 : label is "XLXI_16_10";
begin
   A2DINTRP <= A2DINT(0);
   CAL_OFFSET <= not calnot;
   XLXI_1 : FD16CE_MXILINX_sysctrl
      port map (C=>SIOWN,
                CE=>SYSCTL,
                CLR=>XLXN_23,
                D(15 downto 0)=> BSD(15 downto 0),
                Q(15 downto 0)=> calnot(15 downto 0));
   
   XLXI_3 : BUFE8_MXILINX_sysctrl
      port map (E=>XLXN_21,
                I(7 downto 0)=>A2DINT(7 downto 0),
                O(7 downto 0)=>BSD(7 downto 0));
   
   XLXI_8 : AND2
      port map (I0=>SIOR,
                I1=>SYSCTL,
                O=>XLXN_21);
   
   XLXI_9 : GND
      port map (G=>XLXN_23);
   
--   XLXI_15 : M8_1E_MXILINX_sysctrl
--      port map (D0=>A2DINT(0),
--                D1=>A2DINT(1),
--                D2=>A2DINT(2),
--                D3=>A2DINT(3),
--                D4=>A2DINT(4),
--                D5=>A2DINT(5),
--                D6=>A2DINT(6),
--                D7=>A2DINT(7),
--                E=>XLXN_43,
--                S0=>XLXN_40,
--                S1=>XLXN_41,
--                S2=>XLXN_42,
--                O=>A2DINTRP);
--   
--   XLXI_16 : FD4CE_MXILINX_sysctrl
--      port map (C=>SIOWN,
--                CE=>FIFOSTAT,
--                CLR=>XLXN_52,
--                D0=>BSD(0),
--                D1=>BSD(1),
--                D2=>BSD(2),
--                D3=>XLXN_52,
--                Q0=>XLXN_40,
--                Q1=>XLXN_41,
--                Q2=>XLXN_42,
--                Q3=>open);
   
   XLXI_17 : GND
      port map (G=>XLXN_52);
   
   XLXI_19 : VCC
      port map (P=>XLXN_43);
   
end BEHAVIORAL;



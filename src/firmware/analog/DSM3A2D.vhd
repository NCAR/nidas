--------------------------------------------------------------------------------
-- Copyright (c) 1995-2003 Xilinx, Inc.
-- All Right Reserved.
--------------------------------------------------------------------------------
--   ____  ____ 
--  /   /\/   / 
-- /___/  \  /    Vendor: Xilinx 
-- \   \   \/     Version : 7.1.04i
--  \   \         Application : sch2vhdl
--  /   /         Filename : DSM3A2D.vhf
-- /___/   /\     Timestamp : 02/10/2006 14:47:21
-- \   \  /  \ 
--  \___\/\___\ 
--
--Command: C:/Xilinx/bin/nt/sch2vhdl.exe -intstyle ise -family xc9500 -flat -suppress -w DSM3A2D.sch DSM3A2D.vhf
--Design Name: DSM3A2D
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

entity DSM3A2D is
   port ( AEN        : in    std_logic;
	       A2DINT     : in    std_logic_vector (7 downto 0); 
          BRDSEL     : in    std_logic; 
          FIFOAFAE   : in    std_logic; 
          FIFOEMPTYN : in    std_logic; 
          FIFOFULL   : in    std_logic; 
          FIFOHF     : in    std_logic; 
          IORN       : in    std_logic; 
          IOWN       : in    std_logic; 
          ONEPPS     : in    std_logic; 
          PLLOUT     : in    std_logic; 
          clk        : in    std_logic; 
          SA0        : in    std_logic; 
          SA1        : in    std_logic; 
          SA2        : in    std_logic; 
          SA3        : in    std_logic; 
          SA4        : in    std_logic; 
          A2DCLK     : out   std_logic; 
          A2DCS0N    : out   std_logic; 
          A2DCS1N    : out   std_logic; 
          A2DCS2N    : out   std_logic; 
          A2DCS3N    : out   std_logic; 
          A2DCS4N    : out   std_logic; 
          A2DCS5N    : out   std_logic; 
          A2DCS6N    : out   std_logic; 
          A2DCS7N    : out   std_logic; 
          A2DINTRP   : out   std_logic; 
          A2DRS      : out   std_logic; 
          A2DRWN     : out   std_logic; 
          A2DSYNC    : out   std_logic; 
          CAL_OFFSET : out   std_logic_vector (15 downto 0); 
			       D2ACAL     : out   std_logic_vector (4 downto 0);
--          D2ARWN     : out   std_logic; 
--          D2A0ABN    : out   std_logic; 
--          D2A0DS1    : out   std_logic; 
--          D2A0DS2    : out   std_logic; 
--          D2A1ABN    : out   std_logic; 
--          D2A1DS1    : out   std_logic; 
--          D2A1DS2    : out   std_logic; 
--          D2A2ABN    : out   std_logic; 
--          D2A2DS1    : out   std_logic; 
--          D2A2DS2    : out   std_logic; 
          FIFOCLRN   : out   std_logic; 
          FIFODAFN   : out   std_logic; 
          FIFOLDCK   : out   std_logic; 
          FIFOOE     : out   std_logic; 
          FIFOUNCK   : out   std_logic; 
          IOCS16N    : out   std_logic; 
          I2CSCL     : out   std_logic; 
          PLLDBN     : out   std_logic; 
          SIORN      : out   std_logic; 
          SIOW       : out   std_logic; 
--          TEST43     : out   std_logic; 
--          TEST45     : out   std_logic; 
          SDIN       : out   std_logic;
          CLKIN      : out   std_logic;
          FSIN       : out   std_logic;
          LDAC       : out   std_logic;
          BRDSELO    : out   std_logic;
          A2DBUS     : inout std_logic_vector (15 downto 0); 
          BSD        : inout std_logic_vector (15 downto 0); 
          I2CSDA     : inout std_logic);
end DSM3A2D;

architecture BEHAVIORAL of DSM3A2D is
   signal A2DDATA        : std_logic;
   signal A2DIOEBL       : std_logic;
   signal A2DSTAT        : std_logic;
   signal D2A0           : std_logic;
   signal D2A1           : std_logic;
   signal D2A2           : std_logic;
   signal FIFO           : std_logic;
   signal FIFOCTL        : std_logic_vector (7 downto 0);
   signal FIFOSTAT       : std_logic;
   signal LBSD3          : std_logic;
   signal PRESYNC        : std_logic;
   signal SIOR           : std_logic;
   signal SIORW          : std_logic;
   signal SIOWN          : std_logic;
   signal SYSCTL         : std_logic;
   signal A2DINTRP_DUMMY : std_logic;
   signal A2DSYNC_DUMMY  : std_logic;
   signal SIOW_DUMMY     : std_logic;
   signal BRDSELO_DUMMY  : std_logic;
   signal A2DRS_DUMMY    : std_logic;

	attribute keep: string;
	attribute keep of BRDSELO: signal is "yes";
   component d2aio
     port ( AEN         : in    std_logic;
            D2A0        : in    std_logic; 
            D2A1        : in    std_logic; 
            D2A2        : in    std_logic; 
            SA0         : in    std_logic; 
            SA1         : in    std_logic; 
            SA2         : in    std_logic; 
            BRDSEL      : in    std_logic; 
            SIOW        : in    std_logic; 
            clk         : in    std_logic; -- 10 KHz
            BSD         : in    std_logic_vector (15 downto 0);
            D2ACAL      : out   std_logic_vector (4 downto 0);
            SDIN        : out   std_logic;
            CLKIN       : out   std_logic;
            FSIN        : out   std_logic;
            LDAC        : out   std_logic;
            BRDSELO     : out   std_logic);
   end component;
   
   component fifoio
      port ( SIOR       : in    std_logic; 
             FIFOHF     : in    std_logic; 
             FIFOAFAE   : in    std_logic; 
             FIFOEMPTYN : in    std_logic; 
             FIFOFULL   : in    std_logic; 
             FIFOSTAT   : in    std_logic; 
             SIOWN      : in    std_logic; 
             FIFO       : in    std_logic; 
             ONEPPS     : in    std_logic; 
             A2DSYNC    : in    std_logic; 
             PRESYNC    : in    std_logic; 
             clk        : in    std_logic;
             BSD        : inout std_logic_vector (15 downto 0); 
             FIFOCLRN   : out   std_logic; 
             FIFODAFN   : out   std_logic; 
             FIFOOE     : out   std_logic; 
             FIFOCTL    : out   std_logic_vector (7 downto 0); 
             FIFOUNCK   : out   std_logic); 
--             TEST43     : out   std_logic);
   end component;
   
   component isaintfc
      port ( SA4      : in    std_logic; 
             SA3      : in    std_logic; 
             SA2      : in    std_logic; 
             SA1      : in    std_logic; 
             SA0      : in    std_logic; 
             IOWN     : in    std_logic; 
             IORN     : in    std_logic; 
             BRDSELO  : in    std_logic;
             PLLOUT   : in    std_logic; 
             FIFOCTL  : in    std_logic_vector (7 downto 0); 
             BSD      : inout std_logic_vector (15 downto 0); 
             I2CSDA   : inout std_logic; 
             SIOW     : out   std_logic; 
             SIOR     : out   std_logic; 
             SIOWN    : out   std_logic; 
             SIORN    : out   std_logic; 
             A2DSTAT  : out   std_logic; 
             A2DDATA  : out   std_logic; 
             D2A0     : out   std_logic; 
             D2A1     : out   std_logic; 
             D2A2     : out   std_logic; 
             SYSCTL   : out   std_logic; 
             FIFOSTAT : out   std_logic; 
             FIFO     : out   std_logic; 
             IOCS16N  : out   std_logic; 
             SIORW    : out   std_logic; 
             LBSD3    : out   std_logic; 
             I2CSCL   : out   std_logic);
   end component;
   
   component a2dstatio
     port ( A2DIOEBL   : in    std_logic; 
            PLLOUT     : in    std_logic; 
            SIOR       : in    std_logic; 
            SIOW       : in    std_logic; 
            A2DRS      : in    std_logic;
            FIFOCTL    : in    std_logic_vector (7 downto 0); 
            PLLDBN     : out   std_logic; 
--            TEST45   : out   std_logic; 
            A2DBUS     : inout std_logic_vector (15 downto 0); 
            BSD        : inout std_logic_vector (15 downto 0));
	
--      port ( SIOW     : in    std_logic; 
--             SIOR     : in    std_logic; 
--             A2DIOEBL : in    std_logic; 
--             PLLOUT   : in    std_logic; 
--	            A2DRS    : in    std_logic;
--             A2DBUS   : inout std_logic_vector (15 downto 0); 
--             BSD      : inout std_logic_vector (15 downto 0); 
--             PLLDBN   : out   std_logic); 
--             TEST45   : out   std_logic);
   end component;
   
   component sysctrl
      port ( SYSCTL     : in    std_logic; 
             SIOWN      : in    std_logic; 
             A2DINT     : in    std_logic_vector (7 downto 0); 
             SIOR       : in    std_logic; 
             FIFOSTAT   : in    std_logic; 
             BSD        : inout std_logic_vector (15 downto 0); 
             CAL_OFFSET : out   std_logic_vector (15 downto 0); 
             A2DINTRP   : out   std_logic);
   end component;
   
   component a2dtiming
      port ( A2DSTAT  : in    std_logic; 
             FIFOCTL  : in    std_logic_vector (7 downto 0); 
             SA3      : in    std_logic; 
             SA2      : in    std_logic; 
             SA1      : in    std_logic; 
             SIORW    : in    std_logic; 
             ONEPPS   : in    std_logic; 
             LBSD3    : in    std_logic; 
             A2DINTRP : in    std_logic; 
             PLLOUT   : in    std_logic; 
             D2A0     : in    std_logic; 
             A2DCS0N  : out   std_logic; 
             A2DCS1N  : out   std_logic; 
             A2DCS2N  : out   std_logic; 
             A2DCS3N  : out   std_logic; 
             A2DCS4N  : out   std_logic; 
             A2DCS5N  : out   std_logic; 
             A2DCS6N  : out   std_logic; 
             A2DCS7N  : out   std_logic; 
             A2DIOEBL : out   std_logic; 
             A2DCLK   : out   std_logic; 
             A2DRS    : out   std_logic; 
             A2DRWN   : out   std_logic; 
             A2DSYNC  : out   std_logic; 
             FIFOLDCK : out   std_logic; 
             PRESYNC  : out   std_logic);
   end component;
   
begin
   A2DINTRP <= A2DINTRP_DUMMY;
   A2DSYNC <= A2DSYNC_DUMMY;
   SIOW <= SIOW_DUMMY;
   BRDSELO <= BRDSELO_DUMMY;
   A2DRS <= A2DRS_DUMMY;
   
   XLXI_4 : d2aio
      port map( AEN=>AEN,
                D2A0=>D2A0,    
                D2A1=>D2A1,
                D2A2=>D2A2,
                SA0=>SA0,
                SA1=>SA1,
                SA2=>SA2,
                BRDSEL=>BRDSEL,
                SIOW=>SIOW_DUMMY,
                clk=>clk,
                BSD(15 downto 0)=>BSD(15 downto 0),
                D2ACAL(4 downto 0) => D2ACAL(4 downto 0),
                SDIN=>SDIN,
                CLKIN=>CLKIN,
                FSIN=>FSIN,
                LDAC=>LDAC,
                BRDSELO=>BRDSELO_DUMMY);
   
   XLXI_10 : fifoio
      port map (A2DSYNC=>A2DSYNC_DUMMY,
                FIFO=>FIFO,
                FIFOAFAE=>FIFOAFAE,
                FIFOEMPTYN=>FIFOEMPTYN,
                FIFOFULL=>FIFOFULL,
                FIFOHF=>FIFOHF,
                FIFOSTAT=>FIFOSTAT,
                ONEPPS=>ONEPPS,
                PRESYNC=>PRESYNC,
                SIOR=>SIOR,
                SIOWN=>SIOWN,
                clk=>clk,
                FIFOCLRN=>FIFOCLRN,
                FIFOCTL(7 downto 0)=>FIFOCTL(7 downto 0),
                FIFODAFN=>FIFODAFN,
                FIFOOE=>FIFOOE,
                FIFOUNCK=>FIFOUNCK,
--                TEST43=>TEST43,
                BSD(15 downto 0)=>BSD(15 downto 0));
   
   XLXI_11 : isaintfc
      port map (BRDSELO=>BRDSELO_DUMMY,
                FIFOCTL(7 downto 0)=>FIFOCTL(7 downto 0),
                IORN=>IORN,
                IOWN=>IOWN,
                SA0=>SA0,
                SA1=>SA1,
                SA2=>SA2,
                SA3=>SA3,
                SA4=>SA4,
                PLLOUT=>PLLOUT,
                A2DDATA=>A2DDATA,
                A2DSTAT=>A2DSTAT,
                D2A0=>D2A0,
                D2A1=>D2A1,
                D2A2=>D2A2,
                FIFO=>FIFO,
                FIFOSTAT=>FIFOSTAT,
                IOCS16N=>IOCS16N,
                I2CSCL=>I2CSCL,
                LBSD3=>LBSD3,
                SIOR=>SIOR,
                SIORN=>SIORN,
                SIORW=>SIORW,
                SIOW=>SIOW_DUMMY,
                SIOWN=>SIOWN,
                SYSCTL=>SYSCTL,
                BSD(15 downto 0)=>BSD(15 downto 0),
                I2CSDA=>I2CSDA);
   
   XLXI_14 : a2dstatio
      port map (A2DIOEBL=>A2DIOEBL,
                PLLOUT=>PLLOUT,
                SIOR=>SIOR,
                SIOW=>SIOW_DUMMY,
                A2DRS=>A2DRS_DUMMY,
                FIFOCTL(7 downto 0)=>FIFOCTL(7 downto 0),
                PLLDBN=>PLLDBN,
--                TEST45=>TEST45,
                A2DBUS(15 downto 0)=>A2DBUS(15 downto 0),
                BSD(15 downto 0)=>BSD(15 downto 0));
   
   XLXI_17 : sysctrl
      port map (A2DINT(7 downto 0)=>A2DINT(7 downto 0),
                FIFOSTAT=>FIFOSTAT,
                SIOR=>SIOR,
                SIOWN=>SIOWN,
                SYSCTL=>SYSCTL,
                A2DINTRP=>A2DINTRP_DUMMY,
                CAL_OFFSET(15 downto 0)=>CAL_OFFSET(15 downto 0),
                BSD(15 downto 0)=>BSD(15 downto 0));
   
   XLXI_21 : a2dtiming
      port map (A2DINTRP=>A2DINTRP_DUMMY,
                A2DSTAT=>A2DSTAT,
                FIFOCTL(7 downto 0)=>FIFOCTL(7 downto 0),
                LBSD3=>LBSD3,
                ONEPPS=>ONEPPS,
                PLLOUT=>PLLOUT,
                D2A0=>D2A0,    
                SA1=>SA1,
                SA2=>SA2,
                SA3=>SA3,
                SIORW=>SIORW,
                A2DCLK=>A2DCLK,
                A2DCS0N=>A2DCS0N,
                A2DCS1N=>A2DCS1N,
                A2DCS2N=>A2DCS2N,
                A2DCS3N=>A2DCS3N,
                A2DCS4N=>A2DCS4N,
                A2DCS5N=>A2DCS5N,
                A2DCS6N=>A2DCS6N,
                A2DCS7N=>A2DCS7N,
                A2DIOEBL=>A2DIOEBL,
                A2DRS=>A2DRS_DUMMY,
                A2DRWN=>A2DRWN,
                A2DSYNC=>A2DSYNC_DUMMY,
                FIFOLDCK=>FIFOLDCK,
                PRESYNC=>PRESYNC);
   
end BEHAVIORAL;



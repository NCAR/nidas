--LIBRARY ieee;
--USE ieee.std_logic_1164.ALL;

--PACKAGE data_memory is
--  constant memsize: integer := 61;
--  type word is std_logic_vector(15 downto 0);
--  type memory is array(5 downto 0) of word;
--  type memory is array(5 downto 0) of std_logic_vector(15 downto 0);
--END data_memory;

LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;
USE ieee.std_logic_arith.ALL;
USE ieee.std_logic_unsigned.ALL;
--USE WORK.data_memory.ALL;
entity PC104 is port 
(
     -- bus interface signals --
--  BALE: in STD_LOGIC; 
--  TC: in STD_LOGIC; 
--  BOSC: in STD_LOGIC; 
--  RSTDRV: in STD_LOGIC; 	   --System reset
--  REFRES: in STD_LOGIC; 
--  SBHE: in STD_LOGIC; 
--  MASTER: in STD_LOGIC; 
--  SMEMR: in STD_LOGIC;   
--  SMEMW: in STD_LOGIC;  
--  IOCHRDY: inout STD_LOGIC; 
--  MEMCS16: inout STD_LOGIC; 
  IOCS16: inout STD_LOGIC; 
--  IRQ: out STD_LOGIC_VECTOR (15 downto 3); 		-- interrupts
--  DACK: in STD_LOGIC_VECTOR (7 downto 6); 		-- dma acknowledges
--  DRQ: out STD_LOGIC_VECTOR (7 downto 6); 		-- dma requests
  LBE: out STD_LOGIC;
  LBDIR: out STD_LOGIC;
--  SYSCLK: in STD_LOGIC;  --PC104 bus clock PIN 185 GCK3
  SynClk: in STD_LOGIC;  --50 MHz local clock PIN 80 GCK0
  DIS: out STD_LOGIC;
  AEN: in STD_LOGIC;
  IORD: in STD_LOGIC;   
  IOWR: in STD_LOGIC;
  SA: in STD_LOGIC_VECTOR (9 downto 0); 		-- address bus
  SD: inout STD_LOGIC_VECTOR (15 downto 0); 		-- data bus
-- led bits
  LEDS: out STD_LOGIC_VECTOR(5 downto 0);
  strobe: in std_logic;  
  Hadvance: out STD_LOGIC;
  Hreset: out STD_LOGIC;
  Hdata: in STD_LOGIC;
  HistBits: in std_logic_vector(5 downto 0);
  pulse0: in std_logic;
  pulse1: in std_logic;
  latch_count: in std_logic;
  alt_clock: in std_logic;
  radar_alt: in std_logic;
--  ckout: out std_logic;
  ldebug: out std_logic;
  pdebug: out std_logic;
  ndebug: out std_logic;
  cdebug: out std_logic
);
end PC104;

Architecture behavior of PC104 is
  type memory is array(63 downto 0) of std_logic_vector(15 downto 0);
-- 0x220 hex  
  constant Decode : STD_LOGIC_VECTOR (5 downto 0) := "100010"; 
-- misc global signals --
  signal Histogram: memory;
  signal CardSelect: STD_LOGIC:='0';
  signal strobes: std_logic_vector (15 downto 0):=X"0000";	
  signal index: std_logic_vector(5 downto 0):="000000";
  signal hist_index: integer:=0;
  signal hist_data: std_logic_vector(15 downto 0):=X"0000";
  signal count0: std_logic_vector(15 downto 0):=X"0000";
  signal count1: std_logic_vector(15 downto 0):=X"0000";
  signal count0_latch: std_logic_vector(15 downto 0):=X"0000";
  signal count1_latch: std_logic_vector(15 downto 0):=X"0000";
  signal count_clear: std_logic:='0';
  signal altitude: std_logic_vector(0 to 23):=X"000000";
  signal clear_hist: std_logic:='0';
  signal house_data: std_logic_vector(15 downto 0):=X"0000";
  signal house_adv: std_logic:='0';
  signal alt_clock_old1: std_logic:='0';
  signal alt_clock_old2: std_logic:='0';  
  signal pulse0_old1: std_logic:='0';
  signal pulse1_old1: std_logic:='0';
  signal pulse0_old2: std_logic:='0';
  signal pulse1_old2: std_logic:='0';  
  signal latch_count_old1: std_logic:='0';
  signal latch_count_old2: std_logic:='0'; 
  signal strobe_old1: std_logic:='0';
  signal strobe_old2: std_logic:='0'; 
  signal Hdata_old1: std_logic:='0';
  signal Hdata_old2: std_logic:='0';
  signal ledcount: STD_LOGIC_VECTOR (28 downto 0);

--Components:

--  component ledblink is
--  port (
--    clk: in STD_LOGIC;
--    ledx: out STD_LOGIC_VECTOR (5 downto 0)
--  );
--  end component ledblink;

begin

--  gledblink: ledblink port map (
--    clk => SynClk,
--    ledx => LEDS
--  );	

--    DRQ <= "ZZ";
--    IRQ <= "ZZZZZZZZZZZZZ";
--    DIS <= '0';		-- uncomment to leave configuration decode on
    DIS <= '1';		-- uncomment to disable configuration decode

--    MEMCS16 <= 'Z';
--    IOCHRDY <= 'Z';
--  ckout <= SynClk;
  leds(0) <= not ledcount(23);
  leds(1) <= not ledcount(24);
  leds(2) <= not ledcount(25);
  leds(3) <= not ledcount(26);
  leds(4) <= not ledcount(27);	
  leds(5) <= not ledcount(28);

  ledblinker: process (SynClk) 
  begin
    if SynClk'event and SynClk = '1' then
      ledcount <= ledcount + 1;			
    end if;
  end process ledblinker;

  RadarAltimeter: process (SynClk)
  begin
    if rising_edge(SynClk) then
      alt_clock_old2 <= alt_clock_old1;      
      alt_clock_old1 <= alt_clock;
      if alt_clock_old2 = '1' and alt_clock_old1 = '0' then
        altitude <= altitude(1 to 23) & radar_alt; --bit into lsb
--      altitude <= radar_alt & altitude(0 to 22); --bit into msb
      end if;
    end if;
  end process RadarAltimeter;

  PulseCounters: process (SynClk)
  begin
    if rising_edge(SynClk) then
      pulse0_old1 <= pulse0;
      pulse0_old2 <= pulse0_old1;
      pulse1_old1 <= pulse1;
      pulse1_old2 <= pulse1_old1;
      if count_clear = '1' then
        count0 <= X"0000";
        count1 <= X"0000";
        ndebug <= '1';
      else
        ndebug <= '0';
      end if;
      if pulse0_old2 = '0' and pulse0_old1 = '1' then
        count0 <= count0 + 1;
        pdebug <= '1';
      else
        pdebug <= '0';
      end if;
      if pulse1_old2 = '0' and pulse1_old1 = '1' then
        count1 <= count1 + 1;
      end if;    
    end if;
  end process PulseCounters;

  LatchCounts: process (SynCLk)
  begin
    if rising_edge(SynClk) then
      latch_count_old1 <= latch_count;	 
      latch_count_old2 <= latch_count_old1;
      if latch_count_old2 = '0' and latch_count_old1 = '1' then
        count0_latch <= count0;	   
        count1_latch <= count1;
--	   count0 <= X"0000";
--	   count1 <= X"0000";
        count_clear <= '1';
        ldebug <= '1';	   
      else 
        count_clear <= '0';
        ldebug <= '0';
      end if;
    end if;
  end process LatchCounts;

  HouseCounter: process (SynClk,house_adv)
  begin
    if (house_adv = '1') then
      house_data <= X"0000"; 
    elsif rising_edge(SynClk) then
      Hdata_old2 <= Hdata_old1;
      Hdata_old1 <= Hdata;
      if Hdata_old2 = '0' and Hdata_old1 = '1' then
        house_data <= house_data + 1;	   
      end if;
    end if;
  end process HouseCounter;

  CSelect: process (AEN) 
  begin 
    if SA(9 downto 4) = Decode and AEN = '0' then 
      CardSelect <= '1';
      LBE <= '0';
    else
      LBE <= '1';
      CardSelect <= '0';
    end if;
  end process CSelect;	
  
  Localbuffer: process (CardSelect, IORD)
  begin
    if (CardSelect = '1') and (IORD = '0') then
      LBDIR <= '0';
    else
      LBDIR <= '1';
    end if;
  end process Localbuffer;	

  AddDecode: process (SA, AEN, IORD)
  begin
    if SA(3 downto 0) = "0000" and SA(9 downto 4) = Decode and AEN = '0' then
      IOCS16 <= '0';
      if IORD = '0' then 
        SD(15 downto 0) <= strobes(15 downto 0);
      end if;		
    elsif SA(3 downto 0) = "0001" and SA(9 downto 4) = Decode and AEN = '0' then 
      clear_hist <= '1';
      index <= "000000";
      IOCS16 <= 'Z';	       
    elsif SA(3 downto 0) = "0010" and SA(9 downto 4) = Decode and AEN = '0' then       
      IOCS16 <= '0';
      if IORD = '0' then 
        SD <= Histogram(conv_integer(index));
        index <= index + 1;
      end if;     
    elsif SA(3 downto 0) = "0011" and SA(9 downto 4) = Decode and AEN = '0' then
      IOCS16 <= 'Z';
      Hadvance <= '1';
      house_adv <= '1';
    elsif SA(3 downto 0) = "0100" and SA(9 downto 4) = Decode and AEN = '0' then
      IOCS16 <= '0';
      if IORD = '0' then       
        SD <= house_data;
      end if;     
    elsif SA(3 downto 0) = "0101" and SA(9 downto 4) = Decode and AEN = '0' then
      IOCS16 <= 'Z';
      Hreset <= '1';
    elsif SA(3 downto 0) = "0110" and SA(9 downto 4) = Decode and AEN = '0' then 
      IOCS16 <= '0';
      if IORD = '0' then 
        SD(15 downto 0) <= count0_latch(15 downto 0);
        cdebug <='0';    
      end if;
    elsif SA(3 downto 0) = "1000" and SA(9 downto 4) = Decode and AEN = '0' then 
      IOCS16 <= '0';
      if IORD = '0' then 
        SD(15 downto 0) <= count1_latch(15 downto 0); 
      end if;	       
    elsif SA(3 downto 0) = "1010" and SA(9 downto 4) = Decode and AEN = '0' then 
      IOCS16 <= '0';
      if IORD = '0' then 
        SD(15 downto 8) <= altitude(2 to 9);      
        SD(7 downto 1) <= altitude(10 to 16);      
        SD(0) <= altitude(20) or altitude(21) or altitude (22);
      end if;
    else
      Hadvance <= '0';
      house_adv <= '0';
      Hreset <= '0';
      SD(15 downto 0) <= "ZZZZZZZZZZZZZZZZ";
      IOCS16 <= 'Z';
      cdebug <= '1';
    end if;
  end process AddDecode;

  histo: process(SynClk,clear_hist)
  variable i: integer;
  begin
    if (clear_hist = '1') then
      i := 63;
      while i>=32 loop
        Histogram(i) <= X"0000";
        i := i - 1;
      end loop;
      i := 31;
      while i>=0 loop
        Histogram(i) <= X"0000";
        i := i - 1;
      end loop;   
    elsif rising_edge(SynClk) then
      strobe_old2 <= strobe_old1;
      strobe_old1 <= strobe;
      if strobe_old2 = '1' and strobe_old1 = '0' then
        strobes <= strobes + 1;
        hist_index <= conv_integer(Histbits);
        hist_data <= Histogram(hist_index) + 1;
        Histogram(hist_index) <= hist_data;
      end if;
    end if;
  end process histo;
end behavior;


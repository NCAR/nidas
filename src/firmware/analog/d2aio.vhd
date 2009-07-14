--------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.ALL;
use ieee.numeric_std.ALL;
USE ieee.std_logic_arith.ALL;
USE ieee.std_logic_unsigned.ALL;

entity d2aio is
   port ( AEN     : in    std_logic;
	       D2A0    : in    std_logic; 
          D2A1    : in    std_logic; 
          D2A2    : in    std_logic; 
          SA0     : in    std_logic; 
          SA1     : in    std_logic; 
          SA2     : in    std_logic; 
          BRDSEL  : in    std_logic; 
          SIOW    : in    std_logic; 
          clk     : in    std_logic;  -- 50 MHz
          BSD     : in    std_logic_vector (15 downto 0);
			 D2ACAL  : out   std_logic_vector (4 downto 0);
          SDIN    : out   std_logic;
          CLKIN   : out   std_logic;
          FSIN    : out   std_logic;
			 LDAC    : out   std_logic;
			 BRDSELO : out   std_logic
          );
end d2aio;

architecture BEHAVIORAL of d2aio is
  signal d2a0_old2 : std_logic:='0';
  signal d2a0_old1 : std_logic:='0';
  signal d2a1_old2 : std_logic:='0';
  signal d2a1_old1 : std_logic:='0';
--  signal d2a2_old2 : std_logic:='0';
--  signal d2a2_old1 : std_logic:='0';
  signal data_old2 : std_logic:='0';
  signal data_old1 : std_logic:='0';
  signal flag: std_logic:='0';
  signal fsin1: std_logic:='1';
  signal fsin2: std_logic:='1';
  signal shift_data : std_logic_vector(15 downto 0):=X"0000";
  signal shift_count : std_logic_vector(4 downto 0):="00000";
  signal clk_count : std_logic_vector(1 downto 0):="00";
--  signal cal: std_logic_vector(4 downto 0):="00000";
--  signal cal_snatch: std_logic_vector(4 downto 0):="00000";
--  signal cal_flag: std_logic:='0';
--  signal cal1: std_logic:='0';
--  signal cal2: std_logic:='0';
--  signal cal3: std_logic:='0';
--  signal cal4: std_logic:='0';
--  signal cal5: std_logic:='0';
  signal snatch: std_logic_vector(4 downto 0):="00001";
  signal state1: std_logic:='0';
  signal state2: std_logic:='0';
  signal state3: std_logic:='0';
  signal sig_CLKIN: std_logic:='0';
  signal data_index: integer:=0;
  type state_value is (s1,s2);
  signal state: state_value:=s1;
  signal next_state: state_value;

  attribute keep: string;
  attribute keep of AEN: signal is "true";

begin
--  D2ACAL(4 downto 0) <= cal(4 downto 0); 
  D2ACAL(4 downto 0) <= snatch(4 downto 0);
--  D2ACAL(4 downto 0) <= "00001";
  FSIN <= fsin1 and fsin2;
  CLKIN <= sig_CLKIN;

  select_card: process(BRDSEL,AEN)
  begin 
    if BRDSEL = '0' and AEN = '0' then
	    BRDSELO <= '0';
	  else
	    BRDSELO <= '1';
	  end if;
  end process select_card;
  
--  calproc: process(clk)
--  begin
--    if rising_edge(clk) then
--	   if cal_flag = '1' then
--		  cal <= cal_snatch;
--		  cal_flag <= '0';
--      elsif snatch = "00001" and cal1 = '0' then
--		  cal_snatch <= "00000";
--		  cal <= "00001";
--		  cal_flag <= '1';
--        cal1 <= '1';
--        cal2 <= '0';
--	       cal3 <= '0';
--        cal4 <= '0';
--        cal5 <= '0';
--      elsif snatch = "00010" and cal2 = '0' then
--        cal_snatch <= "00011";
--        cal <= "00001";
--		  cal_flag <= '1';
--        cal2 <= '1';
--        cal1 <= '0';
--        cal3 <= '0';
--        cal4 <= '0';
--        cal5 <= '0';
--		elsif snatch = "00100" and cal3 = '0' then
--        cal_snatch <= "00101";
--		  cal <= "00001";
--		  cal_flag <= '1';
--        cal3 <= '1';
--        cal1 <= '0';
--        cal2 <= '0';
--        cal4 <= '0';
--		    cal5 <= '0';
--      elsif snatch = "01000" and cal4 = '0' then
--        cal_snatch <= "01001";
--        cal <= "00001";
--        cal_flag <= '1';
--        cal4 <= '1';
--        cal1 <= '0';
--        cal2 <= '0';
--        cal3 <= '0';
--        cal5 <= '0';
--		elsif snatch = "10000" and cal5 = '0' then
--        cal_snatch <= "10001";
--        cal <= "00001";
--        cal_flag <= '1';
--        cal5 <= '1';
--        cal1 <= '0';
--        cal2 <= '0';
--        cal3 <= '0';
--        cal4 <= '0';
--		else
--        cal_snatch <= "00001";
--		  cal <= "00001";
--		  cal_flag <= '1';
----        cal1 <= '0';
----        cal2 <= '0';
----        cal3 <= '0';
----        cal4 <= '0';
----        cal5 <= '0';
--      end if;
--	 end if;
--  end process calproc;
  
  dacproc: process(clk)
  begin
    if rising_edge(clk) then		
      d2a0_old2 <= d2a0_old1;
      d2a0_old1 <= D2A0;
      d2a1_old2 <= d2a1_old1;
      d2a1_old1 <= D2A1;
--      d2a2_old2 <= d2a2_old1;
--      d2a2_old1 <= D2A2;
	   data_old2 <= data_old1;
	   data_old1 <= SIOW;
		
      if d2a0_old2 = '0' and d2a0_old1 ='1' then
	      state1 <= '1';
      elsif BRDSEL = '0' and AEN = '0' and SA0 = '0' and SA1 = '0' and SA2 = '0' then
        if data_old2 = '0' and data_old1 = '1' then
          shift_data <= BSD;
          state2 <= state1;
        end if;
      else
        if state3 = '1' then 
          state1 <= '0';
          state2 <= '0';
        end if;	 
      end if;
		
      if d2a1_old2 = '0' and d2a1_old1 ='1' and flag = '0' then
        fsin1 <= '0';
		    LDAC <= '1';		  
		  elsif d2a1_old2 = '1' and d2a1_old1 ='0' and flag = '0' then
		    flag <= '1';
		    fsin1 <= '1';
		    LDAC <= '1';		  
      elsif d2a1_old2 = '0' and d2a1_old1 ='1' and flag = '1' then
		    LDAC <= '0';
		    fsin1 <= '1';
		  elsif d2a1_old2 = '1' and d2a1_old1 ='0' and flag = '1' then
		    flag <= '0';
		    LDAC <= '1';		  
		    fsin1 <= '1';
		  else
		    fsin1 <= '1';
		    LDAC <= '1';
      end if;
    end if;
  end process dacproc;
  
  busproc: process(clk)
  begin
    if rising_edge(clk) then		
      if D2A2 = '1' and (BRDSEL = '0' and AEN = '0' and SA0 = '0' and SA1 = '0' and SA2 = '0') then
        if data_old2 = '0' and data_old1 = '1' then
  		    snatch (4 downto 0) <= BSD(4 downto 0);
--  		    snatch (4 downto 0) <= BSD(15 downto 11);
        end if;  
      else
		    snatch(4 downto 0) <= snatch(4 downto 0);
      end if;
    end if;  
  end process busproc;

  states: process(clk)
  begin
    if falling_edge(clk) then
	   state <= next_state;
    end if;
  end process states;
  
  machine: process(sig_CLKIN)
  begin
    if rising_edge(sig_CLKIN) then
      case state is
      when s1 =>  -- latch data
        shift_count <= "00000";
	     data_index <= 0;
        state3 <= '0';
        fsin2 <= '1';
        SDIN <= '0';
		  if state1 = '1' and state2 = '1' then
          next_state <= s2;
        else
          next_state <= s1;
        end if;        
      when s2 =>
        if shift_count = "10000" then
          state3 <= '1';
          fsin2 <= '1';
          SDIN <= '0';
          shift_count <= "00000";
          data_index <= 0;
          next_state <= s1;
        else
          state3 <= '0';
          fsin2 <= '0';
          data_index <= data_index + 1;
          SDIN <= shift_data(15 - data_index);
          shift_count <= shift_count + 1;
          next_state <= s2;
        end if; 
      end case;
	 end if;   
 end process machine;
 
  DAC_CLK: process(clk)
  begin
    if rising_edge (clk) then
		if clk_count = "00" then
		  sig_CLKIN <= '1';
		  clk_count <= clk_count + 1;
		elsif clk_count = "01" then
		  sig_CLKIN <= '1';
		  clk_count <= clk_count + 1;
		elsif clk_count = "10" then
		  sig_CLKIN <= '0';
		  clk_count <= clk_count + 1;
		elsif clk_count = "11" then
		  sig_CLKIN <= '0';
		  clk_count <= "00";
		else
		  sig_CLKIN <= '0';
		  clk_count <= "00";
      end if;
    end if;
  end process DAC_CLK;  
  
end BEHAVIORAL;



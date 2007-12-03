--Copyright (c) 2005 National Center For Atmospheric Research All rights reserved;
LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;
USE ieee.std_logic_arith.ALL;
USE ieee.std_logic_unsigned.ALL;

entity mux is
  port(
	add_bus_hi:in std_logic;
	add_bus_lo:in std_logic_vector(3 downto 0);
	data_bus:inout std_logic_vector(7 downto 0);
	clkin:in std_logic; -- 24 MHz
	psen:in std_logic;
	rd:in std_logic;
	wr:in std_logic;
	reset:in std_logic;
	data_bit:in std_logic;
        Hdata: in std_logic;
        Shadwor: in std_logic;
	load512:in std_logic;
	tasbase:in std_logic;

	ram_ce:out std_logic;
	ram_rd:out std_logic;
	ram_wr:out std_logic;
	tas:out std_logic;
	bitshift:inout std_logic;
	unld_shift:out std_logic;
	porta:out std_logic;
        out512:out std_logic;
        out512not:out std_logic;
        Hadvance: out std_logic;
        Hreset: out std_logic;
	PA1: out std_logic;
	PA2: out std_logic;
	PA3: out std_logic;
	oscdiv: out std_logic;
        debug:out std_logic);
   
end mux;

Architecture behavior of mux is

  signal sreset: std_logic:='0';
  signal progtas : std_logic;
  signal tdiv : std_logic_vector(1 downto 0);
  signal tcnt: std_logic_vector(1 downto 0);
  signal clkdiv: std_logic_vector(2 downto 0);
  signal clkbit: std_logic; -- 4 MHz unsymetric
  signal clkdiv8: std_logic_vector(3 downto 0);
  signal clkdiv4: std_logic_vector(2 downto 0);
  signal bitclk: std_logic; -- 2 MHz symetric
  signal unldshift: std_logic; -- one pulse per 32 bitclk
  signal byteread: std_logic;
  signal data: std_logic_vector(7 downto 0);
  signal unld_state: std_logic;
  type   state_value is (s0,s1,s2,s3);
  signal state: state_value;
  signal next_state: state_value;
  signal porta_int: std_logic;
  signal bitflag: std_logic;
  signal byteread_state: std_logic;
  signal house_data: std_logic_vector(15 downto 0):=X"0000";
  signal shdwor: std_logic_vector(15 downto 0):=X"0000";
  signal Hdata_old2: std_logic:='0';
  signal Hdata_old1: std_logic:='0';
  signal h_reset: std_logic:='0';
  signal shdwor_old2: std_logic:='0';
  signal shdwor_old1: std_logic:='0';
  signal h_advance: std_logic:='0';
  signal hread0: std_logic;
  signal hread1: std_logic;
  signal sread0: std_logic;
  signal sread1: std_logic;
  signal load_old2: std_logic:='0';
  signal load_old1: std_logic:='0';
  signal loadflag: std_logic:='0';
  signal ushift_old2: std_logic:='0';
  signal ushift_old1: std_logic:='0';
  signal bread_old2: std_logic:='0';
  signal bread_old1: std_logic:='0';
  signal bitclk_old2: std_logic:='0';
  signal bitclk_old1: std_logic:='0';
  signal clkunld: std_logic_vector(2 downto 0):="000";
  signal flagunld: std_logic:='0';
  signal h_rstflag: std_logic:='0';
  signal h_advflag: std_logic:='0';
  signal hcnt: std_logic_vector(3 downto 0):="0000";
--  signal clk512: std_logic_vector(2 downto 0):="000";
--  signal flag512: std_logic:='0';

begin

  ram_ce <= add_bus_hi;
  ram_rd <= rd and psen;
  ram_wr <= wr;

  PA1 <= byteread_state;
  PA2 <= unld_state;
  PA3 <= porta_int;

--0x000
  progtas <= psen and add_bus_hi 
             and not add_bus_lo(3) 
             and not add_bus_lo(2) 
             and not add_bus_lo(1)
             and not add_bus_lo(0)
             and not wr;
--0x001
  unldshift <= psen and add_bus_hi 
             and not add_bus_lo(3) 
             and not add_bus_lo(2) 
             and not add_bus_lo(1)
             and add_bus_lo(0)              
             and not wr;
--0x010
  byteread <= psen and add_bus_hi 
             and not add_bus_lo(3) 
             and not add_bus_lo(2) 
             and add_bus_lo(1)
             and not add_bus_lo(0)              
             and not rd;
--0x011
  debug <= psen and add_bus_hi 
             and not add_bus_lo(3) 
             and not add_bus_lo(2) 
             and add_bus_lo(1)
             and add_bus_lo(0)              
             and not wr;
    
--0x100
  sreset <= psen and add_bus_hi 
             and not add_bus_lo(3) 
             and add_bus_lo(2) 
             and not add_bus_lo(1)
             and not add_bus_lo(0)              
             and not wr;
--0x101
  h_reset <= psen and add_bus_hi
             and not add_bus_lo(3) 
             and add_bus_lo(2) 
             and not add_bus_lo(1)
             and add_bus_lo(0)              
             and not wr;
--0x110
  h_advance <= psen and add_bus_hi
             and not add_bus_lo(3) 
             and add_bus_lo(2) 
             and add_bus_lo(1)
             and not add_bus_lo(0)              
             and not wr;
--0x111
  hread0 <= psen and add_bus_hi 
             and not add_bus_lo(3) 
             and add_bus_lo(2) 
             and add_bus_lo(1)
             and add_bus_lo(0)              
             and not rd;

--0x1000
  hread1 <= psen and add_bus_hi 
             and add_bus_lo(3) 
             and not add_bus_lo(2) 
             and not add_bus_lo(1)
             and not add_bus_lo(0)              
             and not rd;

--0x1001
  sread0 <= psen and add_bus_hi 
             and add_bus_lo(3) 
             and not add_bus_lo(2) 
             and not add_bus_lo(1)
             and add_bus_lo(0)              
             and not rd;

--0x1010
  sread1 <= psen and add_bus_hi 
             and add_bus_lo(3) 
             and not add_bus_lo(2) 
             and add_bus_lo(1)
             and not add_bus_lo(0)              
             and not rd;

  unld_shift <= flagunld and not loadflag;
  out512 <= load512;
  out512not <= not load512;
  porta <= porta_int;
  Hreset <= not h_rstflag;
  Hadvance <= not h_advflag;

  grabsig: process(clkin,reset,sreset)
  begin
    if reset = '1' or sreset = '1' then
      unld_state <= '0';
      byteread_state <= '0';
      loadflag <= '0';
    else
      if falling_edge(clkin) then
        ushift_old2 <= ushift_old1;
        ushift_old1 <= unldshift;
        bread_old2 <= bread_old1;
        bread_old1 <= byteread;
        load_old2 <= load_old1;
        load_old1 <= load512;
        if load_old2 = '1' and load_old1 = '0' then
          loadflag <= '1';
        end if;

        if ushift_old1 = '1' and ushift_old2 = '0' then
          unld_state <= '1';
          byteread_state <= byteread_state;
        elsif bread_old1 = '1' and bread_old2 = '0' then
          byteread_state <= '1';
          unld_state <= '0';
          loadflag <= '0';
        elsif porta_int = '0' then
          byteread_state <= '0';
          unld_state <= unld_state;
        else
          byteread_state <= byteread_state;
          unld_state <= unld_state;
          loadflag <= loadflag;
        end if;
      end if;
    end if;
  end process grabsig;


  databus: process(reset,sreset,clkin)
  begin
    if reset = '1' or sreset = '1' then
      tdiv(1 downto 0) <= "00";
      hcnt(3 downto 0) <= "0000";
      h_rstflag <= '0';
      h_advflag <= '0';
    elsif rising_edge(clkin) then
      Hdata_old2 <= Hdata_old1;
      Hdata_old1 <= Hdata;
      shdwor_old2 <= shdwor_old1;
      shdwor_old1 <= Shadwor;
      if h_rstflag = '1' then
        if hcnt = "1100" then  --Hold reset to probe
          hcnt <= "0000";
          h_rstflag <= '0';
        else
          hcnt <= hcnt + 1;
          h_rstflag <= h_rstflag;
        end if;
      end if;
      if h_advflag = '1' then
        if hcnt = "1100" then  --Hold advance to probe
          hcnt <= "0000";
          h_advflag <= '0';
        else
          hcnt <= hcnt + 1;
          h_advflag <= h_advflag;
        end if;
      end if;
      if h_reset = '1' then
        h_rstflag <= '1';
        house_data <= X"0000"; 
      elsif h_advance = '1' then
        h_advflag <= '1';
        house_data <= X"0000"; 
      elsif Hdata_old2 = '0' and Hdata_old1 = '1' then
        house_data <= house_data + 1;	   
      elsif shdwor_old2 = '0' and shdwor_old1 = '1' then
        shdwor <= shdwor + 1;	   
      elsif hread0 = '1' then
        data_bus(7 downto 0) <= house_data(7 downto 0);
      elsif hread1 = '1' then
        data_bus(7 downto 0) <= house_data(15 downto 8);
      elsif sread0 = '1' then
        data_bus(7 downto 0) <= shdwor(7 downto 0);
        shdwor(15 downto 0) <= X"0000";
      elsif sread1 = '1' then
        data_bus(7 downto 0) <= shdwor(15 downto 8);
      elsif progtas = '1' then
        tdiv(1 downto 0) <= data_bus(1 downto 0);
      elsif byteread = '1' then
        data_bus(7 downto 0) <= data(7 downto 0);
      else 
        data_bus(7 downto 0) <= "ZZZZZZZZ";
        tdiv(1 downto 0) <= tdiv(1 downto 0);
      end if;
    end if;
  end process databus;

  clock1: process(clkin, reset, sreset)
  begin
    if reset = '1' or sreset = '1' then
      clkbit <= '0';
      clkdiv(2 downto 0) <= "000";
      bitshift <= '0';
    else
      if rising_edge(clkin) then
        clkdiv <= clkdiv + 1;
        if clkdiv(2 downto 0) = "101" then --sets bitshift frequency
          clkbit <= '1';
          clkdiv(2 downto 0) <= "000";
        else
          clkbit <= '0';
        end if;
      end if;
      if bitflag = '1' then
        bitshift <=  bitclk;
      else 
        bitshift <= '0';
      end if;
    end if;
  end process clock1;

  clock2: process(clkbit, reset, sreset)
  begin
    if reset = '1' or sreset = '1' then
    bitclk <= '0';
  else
    if rising_edge(clkbit)then
      if bitclk = '1' then
        bitclk <= '0';
      else
        bitclk <= '1';
      end if;
    end if;
  end if;
  end process clock2;

  unload: process(clkin,reset,sreset)
  begin
    if reset = '1' or sreset = '1' then
      state <= s0;
    else
      if falling_edge(clkin) then
        state <= next_state;
      end if;
    end if;
  end process unload;

  machine: process(clkin,reset,sreset)
  begin
    if reset = '1' or sreset = '1' then
      next_state <= s0;
      bitflag <= '0';
      clkdiv8 <= "0000";
      clkdiv4 <= "000";
      porta_int <= '0';
      flagunld <= '0';
      clkunld(2 downto 0) <= "000";
    elsif rising_edge(clkin) then
      bitclk_old2 <= bitclk_old1;
      bitclk_old1 <= bitclk;

      case state is
      when s0=>  -- wait for unload
        bitflag <= '0';
        if clkunld(2 downto 0) = "111" then
          clkunld(2 downto 0) <= "000";
          flagunld <= '0';
          next_state <= s1;
        elsif unld_state = '1' then
          next_state <= s0;
          flagunld <= '1';
          clkunld <= clkunld + 1;
        else
          next_state <= s0;
        end if;
      when s1 =>  -- clock 8 bits
        if clkdiv8 = "1000" then
          porta_int <= '1';
          next_state <= s2;
        elsif bitclk_old1 = '1' and bitclk_old2 = '0' then 
          bitflag <= '1';
          clkdiv8 <= clkdiv8 + 1;
          next_state <= s1;
        else
          bitflag <= bitflag;
          next_state <= s1;
        end if;
      when s2 =>	--wait for microp to read byte
        bitflag <= '0';
        clkdiv8 <= "0000";
        if byteread_state = '1' and porta_int = '1' then
          clkdiv4 <= clkdiv4 + 1;
          next_state <= s3;
          porta_int <= '0';
        else
          next_state <= s2;
        end if;
      when s3 =>
        bitflag <= '0';
        if clkdiv4 = "100" then
          clkdiv4 <= "000";
          next_state <= s0; -- go wait for unldshift
        else
          next_state <= s1;
        end if;      
      end case;
    else
      next_state <= next_state;
      porta_int <= porta_int;
      clkdiv8 <= clkdiv8;
      clkdiv4 <= clkdiv4;
      flagunld <= flagunld;
      bitflag <= bitflag;
    end if;  
  end process machine;

  shift_in: process(bitshift,reset)
  begin
    if reset = '1' then
      data <= "00000000";
    elsif falling_edge(bitshift) then
      data <= data(6 downto 0) & data_bit; --shift data in
--      data <= data_bit & data(7 downto 1); --shift data in
    end if;  
  end process shift_in;

  tascnt: process(tasbase, reset, sreset)
  begin 
    if reset = '1' or sreset = '1' then
      tcnt <= "00";
    else
      if rising_edge(tasbase) then
        tcnt <= tcnt + 1;
      end if;
    end if;
  end process tascnt;

  airspeed: process(reset,tdiv,sreset)
  begin  
    if reset = '1' or sreset = '1' then
      tas <= '0';
      oscdiv <= '0';
    else  
    case tdiv(1 downto 0) is
      when "00" =>
        tas <= tasbase;
        oscdiv <= '0';
      when "01" =>
        tas <= tasbase;
        oscdiv <= 'Z';
      when others =>
        oscdiv <= '0';
        tas <= '0';
    end case;
    end if;
  end process airspeed; 

end behavior;

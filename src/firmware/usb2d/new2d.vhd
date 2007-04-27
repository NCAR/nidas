LIBRARY ieee;
USE ieee.std_logic_1164.ALL;
USE ieee.numeric_std.ALL;
USE ieee.std_logic_arith.ALL;
USE ieee.std_logic_unsigned.ALL;

entity mux is
  port(
       in1_bus:in std_logic_vector(15 downto 0);
       in2_bus:in std_logic_vector(15 downto 0);
       in3_bus:in std_logic_vector(15 downto 0);
       in4_bus:in std_logic_vector(15 downto 0);
       fullflag:in std_logic;  
       emptyflag: in std_logic;            
       add_bus_hi:in std_logic;
       add_bus_lo:in std_logic_vector(3 downto 0);
       data_bus:inout std_logic_vector(7 downto 0);
       fd_bus:inout std_logic_vector(15 downto 0);
       clkin:in std_logic; -- 48 MHz
       psen:in std_logic;
       rd:in std_logic;
       wr:in std_logic;
       reset:in std_logic;
       particle: in std_logic;
       dof:in std_logic;

       ram_ce:out std_logic;
       ram_rd:out std_logic;
       ram_wr:out std_logic;
       tasbase:in std_logic;
       tas:out std_logic;
       slwr: out std_logic;
       fifoadr0: out std_logic;
       fifoadr1: out std_logic;
       pktend: out std_logic;
       oscdiv: out std_logic;
       debug:out std_logic);
end mux;

Architecture behavior of mux is

--Below here is 2dmuxusb1
  signal sync_pattern: std_logic_vector(15 downto 0);
  signal time_count: std_logic_vector(39 downto 0);
  signal in1_latch: std_logic_vector(15 downto 0);
  signal in2_latch: std_logic_vector(15 downto 0);
  signal in3_latch: std_logic_vector(15 downto 0);
  signal in4_latch: std_logic_vector(15 downto 0);
  signal particle_flag: std_logic;
  signal tas_old2: std_logic;
  signal tas_old1: std_logic;
  signal sreset: std_logic:='0';
  signal progtas: std_logic;
  signal tdiv: std_logic_vector(1 downto 0);
  signal tcnt: std_logic_vector(1 downto 0);
  signal shdwor: std_logic_vector(15 downto 0):=X"0000";
  signal particle_old2: std_logic:='0';
  signal particle_old1: std_logic:='0';
  signal tasbase_old2: std_logic:='0';
  signal tasbase_old1: std_logic:='0';
  signal sread0: std_logic;
  signal sread1: std_logic;
  signal pkend: std_logic;
  signal div4clk: std_logic;
  signal div8clk: std_logic;
  signal clkcount: std_logic_vector(2 downto 0):="000";
  signal start_flag: std_logic:='0';
  signal stateflag: std_logic;
  signal shadflag: std_logic;
  signal shadflag_old2: std_logic;
  signal shadflag_old1: std_logic;
  signal sync: std_logic:='1';
  type   state_value is (s0,s1,s2,s3,s4,s5,s6,s7,s8,s9,s10,s11,s12,s13,s14,s15);
  signal state: state_value;
  signal next_state: state_value;

begin
  sync_pattern <= "1010101010101010";
  fifoadr0 <= '0';
  fifoadr1 <= '0';
  pktend <= pkend;

--0x000
  progtas <= psen and add_bus_hi 
             and not add_bus_lo(3) 
             and not add_bus_lo(2) 
             and not add_bus_lo(1)
             and not add_bus_lo(0)
             and not wr;
 
--0x011
--  debug <= psen and add_bus_hi 
--             and not add_bus_lo(3) 
--             and not add_bus_lo(2) 
--             and add_bus_lo(1)
--             and add_bus_lo(0)              
--             and not wr;
   
  --0x100
  sreset <= psen and add_bus_hi 
             and not add_bus_lo(3) 
             and add_bus_lo(2) 
             and not add_bus_lo(1)
             and not add_bus_lo(0)              
             and not wr;

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

  clock2: process(clkin,reset,sreset) --Divide 48 MHz by 2 = 24 MHz
  begin
    if reset = '1' or sreset = '1' then
      div4clk <= '0';
      div8clk <= '0';
--      clkcount <= "00";
      clkcount <= "000";
    else
      if rising_edge(clkin) then
        clkcount <= clkcount + 1;
--        if clkcount = "00" then
        if clkcount(0) = '0' and clkcount(1) = '0' then
          div4clk <= '1';
        else
--          if clkcount = "10" then
          if clkcount(0) = '0' and clkcount(1) = '1' then
            div4clk <= '0';
          end if;
        end if;
        if clkcount = "000" then
          div8clk <= '1';
        else
          if clkcount = "100" then
            div8clk <= '0';
          end if;
        end if;
      end if;
    end if;
  end process clock2;

  ram: process(clkin,reset,sreset)
  begin
    if reset = '1' or sreset = '1' then
      ram_ce <= '1';
      ram_rd <= '1';
      ram_wr <= '1';
    else
      ram_ce <= not add_bus_hi;
      ram_rd <= rd or psen;
      ram_wr <= wr;
    end if;
  end process ram;

  nextstate: process(div4clk,reset,sreset)
  begin
    if reset = '1' or sreset = '1' then
      state <= s0;
      if emptyflag = '1' then
        pkend <= '0';
      end if;
    else
      pkend <= '1';
      if falling_edge(div4clk) then
        state <= next_state;
      end if;
    end if;
  end process nextstate;

  machine: process(div4clk,reset,sreset)
  begin
    if reset = '1' or sreset = '1' then
      next_state <= s0;
      slwr <= '1';
    elsif rising_edge(div4clk) then
      tasbase_old2 <= tasbase_old1;
      tasbase_old1 <= tasbase;
      if tasbase_old2 = '0' and tasbase_old1 = '1' then
        in1_latch <= in1_bus;
        in2_latch <= in2_bus;
        in3_latch <= in3_bus;
        in4_latch <= in4_bus;
      end if;
   
      case state is 
        when s0=> -- wait for particle strobe
          if tasbase_old2 = '0' and tasbase_old1 = '1' and 
             particle_flag = '1' then
            if fullflag = '1' then
              fd_bus(15 downto 8) <= in4_latch(7 downto 0);
              fd_bus(7 downto 0) <= in4_latch(15 downto 8);
              slwr <= '0';
              start_flag <= '1';
              next_state <= s1;
            else
              sync <= '0';
              next_state <= s0;
            end if;
          else
            if particle_flag = '0' and start_flag = '1' then
              next_state <= s8;
            else
              next_state <= s0;
            end if;
          end if;
        when s1=>
          slwr <= '1';
          next_state <= s2;
        when s2=>  -- latch data and send to USB fifo
          if fullflag = '1' then
            fd_bus(15 downto 8) <= in3_latch(7 downto 0);
            fd_bus(7 downto 0) <= in3_latch(15 downto 8);
            slwr <= '0';
            next_state <= s3;
          else
            sync <= '0';
            next_state <= s2;
          end if;
        when s3=>
          slwr <= '1';
          next_state <= s4;
        when s4=>  -- latch data and send to USB fifo
          if fullflag = '1' then
            fd_bus(15 downto 8) <= in2_latch(7 downto 0);
            fd_bus(7 downto 0) <= in2_latch(15 downto 8);
            slwr <= '0';
            next_state <= s5;
          else
            sync <= '0';
            next_state <= s4;
          end if;
        when s5=>
          slwr <= '1';
          next_state <= s6;
        when s6=>  -- latch data and send to USB fifo
          if fullflag = '1' then
            fd_bus(15 downto 8) <= in1_latch(7 downto 0);
            fd_bus(7 downto 0) <= in1_latch(15 downto 8);
            slwr <= '0';
            next_state <= s7;
          else
            sync <= '0';
            next_state <= s6;
          end if;
        when s7=>
          slwr <= '1';
          if particle_flag = '0' then
            next_state <= s8;
          else 
            next_state <= s0;
          end if;

        when s8=> -- write sync and time stamp
          if fullflag = '1' then
            start_flag <= '0';
            if sync = '1' then
              fd_bus(15 downto 0) <= sync_pattern(15 downto 0);
            else
              fd_bus(15 downto 0) <= not sync_pattern(15 downto 0);
            end if;
            slwr <= '0';
            next_state <= s9;
          else
            next_state <= s8;
          end if;
        when s9=>
          slwr <= '1';
          next_state <= s10;
        when s10=>  -- latch data and send to USB fifo
          if fullflag = '1' then
--            fd_bus(15 downto 12) <= sync_pattern(15 downto 12);
--            fd_bus(11 downto 8) <= time_count(35 downto 32);
            fd_bus(15 downto 8) <= time_count(39 downto 32);
            fd_bus(7 downto 0) <= sync_pattern(7 downto 0);
            slwr <= '0';
            next_state <= s11;
          else
            next_state <= s10;
          end if;
        when s11=>
          slwr <= '1';
          next_state <= s12;
        when s12=>  -- latch data and send to USB fifo
          if fullflag = '1' then
            fd_bus(15 downto 8) <= time_count(23 downto 16);
            fd_bus(7 downto 0) <= time_count(31 downto 24);
            slwr <= '0';
            next_state <= s13;
          else
            next_state <= s12;
          end if;
        when s13=>
          slwr <= '1';
          next_state <= s14;
        when s14=>  -- latch data and send to USB fifo
          if fullflag = '1' then
            fd_bus(15 downto 8) <= time_count(7 downto 0);
            fd_bus(7 downto 0) <= time_count(15 downto 8);
            slwr <= '0';
            next_state <= s15;
          else
            next_state <= s14;
          end if;
        when s15=>
          sync <= '1';
          slwr <= '1';
          next_state <= s0;
      end case;
    else
      next_state <= next_state;
    end if;
  end process machine;

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

  databus: process(reset,progtas,sreset,sread0,sread1)
  begin
    if reset = '1' or sreset = '1' then
      tdiv(1 downto 0) <= "00";
    end if;
    if progtas = '1' then
      tdiv(1 downto 0) <= data_bus(1 downto 0);
    else 
      tdiv(1 downto 0) <= tdiv(1 downto 0);
    end if;
    if sread0 = '1' then
      data_bus(7 downto 0) <= shdwor(7 downto 0);
      shadflag <= '0';
    elsif sread1 = '1' then
      data_bus(7 downto 0) <= shdwor(15 downto 8);
      shadflag <= '1';
    else 
      data_bus(7 downto 0) <= "ZZZZZZZZ";
    end if;
  end process databus;

  airspeed: process(reset,tdiv,sreset)
  begin  
    if reset = '1' or sreset = '1' then
      tas <= '0';
      oscdiv <= '0';
    else  
    case tdiv(1 downto 0) is
      when "00" =>
        tas <= tcnt(1);
        oscdiv <= '0';
      when "01" =>
        tas <= tcnt(1);
        oscdiv <= 'Z';
      when others =>
        tas <= '0';
    end case;
    end if;
  end process airspeed; 

  ParticleCounter: process (clkin)
  begin
    if falling_edge(clkin) then
      particle_old2 <= particle_old1;
      particle_old1 <= particle;
      shadflag_old2 <= shadflag_old1;
      shadflag_old1 <= shadflag;
      if particle_old2 = '1' and particle_old1 = '0' then
        shdwor <= shdwor + 1;	   
        if dof = '1' then
          debug <= '1';
          particle_flag <= '1';
        end if;
      end if;
      if particle_old2 = '0' and particle_old1 = '1' then
        particle_flag <= '0';
        debug <= '0';
      end if;
      if shadflag_old2 = '0' and shadflag_old1 = '1' then
        shdwor(15 downto 0) <= X"0000";
      end if;
    end if;
  end process ParticleCounter;

--  count_time: process(tasbase,reset,sreset)
  count_time: process(div4clk,reset,sreset)
  begin
    if reset = '1' or sreset = '1' then
--     time_count (37 downto 36) <= "00";
     time_count(39 downto 0) <= X"0000000000";
    else 
      if time_count(39 downto 0) = X"FFFFFFFFFF" then
--        time_count (37 downto 36) <= "00";
        time_count(39 downto 0) <= X"0000000000";  
--      elsif rising_edge(tasbase) then
      elsif rising_edge(div4clk) then
        time_count <= time_count + 1;
      else
        time_count(39 downto 0) <= time_count(39 downto 0);
      end if;
    end if;
  end process count_time;

end behavior;


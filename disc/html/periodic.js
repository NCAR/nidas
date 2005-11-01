var pU = {

  cnt: 1,

  loop: function() {
    var resp = GetClocks();
    if (resp)
      for (name in resp) {
        document.getElementById(name).innerHTML =
          document.getElementById(name).label+' ('+resp[name]+')';
      }
    if (++pU.cnt > 10) {
      pU.cnt=1;
      if (selectedDsm)
        recvResp( GetStatus[selectedDsm]() );
    }
    setTimeout('pU.loop()',1000);
  }
}
window.onload=pU.loop;

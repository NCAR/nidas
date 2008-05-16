var periodic = {

  cnt: 1,

  UpdateClocks: function() {
    var resp = GetClocks();
    if (resp)
      for (name in resp) {
        document.getElementById(name).innerHTML =
          document.getElementById(name).label+' ('+resp[name]+')';
      }
  },

  loop: function() {
    periodic.UpdateClocks();
    if (++periodic.cnt > 3) {
      periodic.cnt=1;
      if (selectedDsm)
        recvResp( GetStatus[selectedDsm]() );
    }
    if (is_periodic)
      setTimeout('periodic.loop()',1000);
  }
}
window.onload=periodic.loop;

function test() {
  var contentWin = window.open("about:blank", "", "width=100,height=100");
  var enumerator = Cc["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Ci.nsIWindowMediator)
                     .getEnumerator("navigator:browser");

  while (enumerator.hasMoreElements()) {
    let win = enumerator.getNext();
    if (win.content == contentWin) {
      gPrefService.setBoolPref("browser.tabs.closeWindowWithLastTab", false);
      win.gBrowser.removeCurrentTab();
      ok(win.closed, "popup is closed");

      // clean up
      if (!win.closed)
        win.close();
      if (gPrefService.prefHasUserValue("browser.tabs.closeWindowWithLastTab"))
        gPrefService.clearUserPref("browser.tabs.closeWindowWithLastTab");

      return;
    }
  }

  throw "couldn't find the content window";
}

#include "TApplication.h"
#include "TGClient.h"
#include "TGButton.h"
#include "TGFrame.h"
#include "TGTextEntry.h"

class MyMainFrame : public TGMainFrame {
public:
   TGTextEntry *fText1, *fText2;

   MyMainFrame(const TGWindow *p, UInt_t w, UInt_t h) : TGMainFrame(p, w, h) {
      SetCleanup(kDeepCleanup);

      fText1 = new TGTextEntry(this, new TGTextBuffer(100));
      AddFrame(fText1, new TGLayoutHints(kLHintsExpandX, 5,5,5,5));

      fText2 = new TGTextEntry(this, new TGTextBuffer(100));
      AddFrame(fText2, new TGLayoutHints(kLHintsExpandX, 5,5,5,5));

      TGTextButton *button = new TGTextButton(this, "&Submit");
      button->Connect("Clicked()", "MyMainFrame", this, "Submit()");
      AddFrame(button, new TGLayoutHints(kLHintsCenterX));

      MapSubwindows();
      Resize(GetDefaultSize());
      MapWindow();
   }

   void Submit() {
      const char *text1 = fText1->GetText();
      const char *text2 = fText2->GetText();
      printf("Text1: %s, Text2: %s\n", text1, text2);
   }
};

void startGUI() {
   TApplication app("app", 0, 0);
   new MyMainFrame(gClient->GetRoot(), 200, 100);
   app.Run();
}

int main() {
   startGUI();
   return 0;
}

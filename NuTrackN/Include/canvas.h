
#ifndef EXAMPLE_H
#define EXAMPLE_H

#include <iostream>
#include <QFileDialog>
#include <QDataStream>
#include <QFile>
#include <fstream>
#include <iomanip>
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <tracknhistogram.h>
#include "Integral.h"
#include "calib.h"
#include "PeakFit.h"
#include "SpectrumImportDialog.h"
#include "EfficiencyDialog.h"
#include <memory>

class MatrixReader;
class HelpDialog;
#include <cstdlib>
#include <cstdio>
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QTextStream>
#include <QGroupBox>
#include <QLabel>
#include <QGridLayout>
#include <QScrollArea>


#include <QWidget>
#include <QPushButton>
#include <QLayout>
#include <QSplitter>
#include <QTimer>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QAction>
#include <QKeySequence>
#include <QFileDialog>
#include <QDataStream>
#include <QFile>
#include <QtMath>
#include <QInputDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QTextBrowser>
#include <QWheelEvent>
#include <QMenu>
#include <QIcon>
#include <QMainWindow>
#include <QDir>
#include <QFileInfo>



#include <TCanvas.h>
#include <TVirtualX.h>
#include <TSystem.h>
#include <TFormula.h>
#include <TF1.h>
#include <TFormula.h>
#include <TFrame.h>
#include <TTimer.h>
#include <TFitResult.h>
#include <TLatex.h>
#include <TLine.h>
#include <TMatrixD.h>
#include <Math/Minimizer.h>
#include <TRandom.h>
#include <THStack.h>
#include <TContextMenu.h>
#include <TObjArray.h>



#include <QLabel>
#include <QPicture>
#include <QPainter>
#include <QMessageBox>
#include <QApplication>
#include <QCloseEvent>
#include <TApplication.h>
#include <TTimer.h>
#include <TGFrame.h>
#include <TSystem.h>
#include <TVirtualX.h>
#include <TEnv.h>
#include <TGClient.h>
#include <TStyle.h>
#include <TColor.h>
#include <TAxis.h>
#include <TList.h>

class TH1F;
class QMainCanvas;
class DisplayParamsDialog;
class AutoCalibDialog;

class QZoomHUD : public QWidget
{
   Q_OBJECT
public:
   explicit QZoomHUD(QWidget *parent = nullptr);
   void updateData(TH1F *hist, int targetBin, double energy = -1.0, bool isCalibrated = false,
                   const std::vector<double> &fitCurve = {},
                   const std::vector<double> &bkgCurve = {},
                   bool hasFit = false,
                   const QString &fitInfo = QString());

protected:
   void paintEvent(QPaintEvent *event) override;

private:
   int                 m_targetBin;
   double              m_targetCounts;
   double              m_targetEnergy;
   bool                m_isCalibrated;
   int                 m_startBin;
   int                 m_endBin;
   double              m_maxCount;
   std::vector<double> m_counts;
   std::vector<double> m_fitCurve;
   std::vector<double> m_bkgCurve;
   bool                m_hasFit;
   QString             m_fitInfo;
};

class QRootCanvas : public QWidget
{
   Q_OBJECT
   friend class QMainCanvas;

public:
   QRootCanvas( QWidget *parent = 0);
   virtual ~QRootCanvas();
   TCanvas* getCanvas() { return fCanvas; }
   QZoomHUD* getZoomHUD() { return m_zoomHUD; }
   void setMainCanvas(QMainCanvas *main) { m_mainCanvas = main; }
   void updateZoomHUD(int mouseX, int mouseY);
   void hideZoomHUD();
   bool eventFilter(QObject *watched, QEvent *event) override;

protected:
   TCanvas        *fCanvas;
   Double_t       xMousePosition, yMousePosition;
   bool           controlKeyIsPressed{false}, aKeyWasPressed{false}, cKeyWasPressed{false}, zKeyWasPressed{false}, mKeyWasPressed{false}, fKeyWasPressed{false}, sKeyWasPressed{false}, dKeyWasPressed{false}, oKeyWasPressed{false};
   QZoomHUD       *m_zoomHUD;
   QMainCanvas    *m_mainCanvas;

   virtual void    mouseMoveEvent( QMouseEvent *e );
   virtual void    mousePressEvent( QMouseEvent *e );
   virtual void    mouseReleaseEvent( QMouseEvent *e );
   virtual void    paintEvent( QPaintEvent *e );
   virtual void    resizeEvent( QResizeEvent *e );
   virtual void    wheelEvent(QWheelEvent *e);
   virtual void    showContextMenu(QMouseEvent *e);
   virtual void    keyPressEvent(QKeyEvent *event);
   virtual void    keyReleaseEvent(QKeyEvent *event);
   virtual void    focusOutEvent(QFocusEvent *event);
   virtual void    enterEvent(QEvent *event);
   virtual void    leaveEvent(QEvent *event);

signals:
   void requestEnCalDialog();
   void requestTrackFitDialog();
   void requestDisplayParamsDialog();
   void requestEfficiencyDialog();
   void requestPeakWidthMode();
   void requestMatrixSetup();
   void requestAutoCalibDialog();
   void requestIntegrationNoBackground();
   void requestIntegrationWithBackground();
   void requestGoToEnergy();
   void requestZoomAroundCursor(Int_t, Int_t);
   void requestAutoIntegration(Int_t, Int_t);
   void autoFitRequested(int, int);
   void requestClearTheScreen();
   void addBackgroundMarkerRequested(Int_t, Int_t);
   void addIntegralMarkerRequested(Int_t, Int_t);
   void deleteBackgroundMarkersRequested();
   void deleteIntegralMarkersRequested();
   void showBackgroundMarkersRequested();
   void showIntegralMarkersRequested();
   void showAllMarkersRequested();
   void requestZoomTheScreen();
   void requesttranslateplusTheScreen();
   void requesttranslateminusTheScreen();
   void requesttranslatedownTheScreen();
   void requesttranslateupTheScreen();
   void fullscreen();
   void requestFullX();
   void requestFullY();
   void requestSameX();
   void requestSameY();
   void requestDeleteBackgroundMarkers();
   void requestDeleteIntegralMarkers();
   void requestDeleteAllMarkers();
   void requestShowBackgroundMarkers();
   void requestShowIntegralMarkers();
   void requestShowAllMarkers();
   void addSpaceBarMarkerRequested(Int_t, Int_t);
   void requestAddRangeMarker(Int_t, Int_t);
   void requestDeleteRangeMarkers();
   void requestShowRangeMarkers();
   void requestAddGaussMarker(Int_t, Int_t);
   void requestDeleteGaussMarkers();
   void requestShowGaussMarkers();
   void addGateMarkerRequested(Int_t, Int_t);
   void requestDeleteGateMarkers();
   void requestShowGateMarkers();
   void requestGateCut();
    void requestFitGauss();
    void requestPeakSearch();
    void requestDeletePeakMarkers();
    void requestShowPeakMarkers();
    void requestHelp();
    void requestToggleLogY();
   void killSwitch();
   void requestMJMarkers();
   void requestMVMarkers();
   void requestQuickCalibration();
   void requestMatrixProjection();
   void requestFitBackground();
   void mousePilgrimCoordRequest(Double_t , Double_t );
   void mouseLeftClickCoordRequest(Double_t , Double_t );
   void AddLineRequest();
   void DeleteLineRequest();
   void AddCulomnRequest();
   void DeleteCulomnRequest();
   void RefreshScreenRequest();
   void showXY(Double_t , Double_t);
   void requestOpenSpectrumDialog();
   void requestExportSpectrumDialog();
   void requestPrintPlot();
   void requestSetYMax(double);
   void requestSetYMin(double);
   void requestCTCalibration();
   void requestATCalibration();
   void requestDeleteZJMarkers();
   void requestDeleteZVMarkers();
   void requestDrawZeroLine();
   void requestShiftDisplayLeft75();
   void requestShiftDisplayRight75();
   void requestDeleteNearestGaussMarker(Int_t, Int_t);
   void requestDefineMacro(int macroId);
   void requestExecuteMacro(int macroId);
   void requestCycleMacro(int macroId);
   void requestShowMacro(int macroId);
   void requestClearMacro(int macroId);
   void requestMacroDialog();
};

struct PeakParamState {
    bool fixCentroid{false};
    double centroidVal{0.0};
    bool fixAmp{false};
    double ampVal{0.0};
    bool fixWidth{false};
    double widthVal{0.0};
};

struct PeakUIControls {
    QGroupBox *groupBox{nullptr};
    QLabel *lblNetArea{nullptr};
    QCheckBox *chkFixCentroid{nullptr};
    QDoubleSpinBox *spinCentroid{nullptr};
    QCheckBox *chkFixAmp{nullptr};
    QDoubleSpinBox *spinAmp{nullptr};
    QCheckBox *chkFixWidth{nullptr};
    QDoubleSpinBox *spinWidth{nullptr};
};

struct DetectedPeak;
class IntegralDialog;

class QMainCanvas : public QWidget
{
   Q_OBJECT

   friend void runAutoFit(QMainCanvas *mainCanvas, int x, int y);
   friend void runMultiPeakFit(QMainCanvas *mainCanvas);
   friend void fitBackgroundHelper(QMainCanvas *mainCanvas);
   friend void showFitParametersDialog(QMainCanvas *mainCanvas, const QString &title, const QString &htmlContent, const std::vector<FittedPeakData> &peaks);
   friend void showPeakSearchParamsDialog(QMainCanvas *mainCanvas, double sigma, double threshold, const std::vector<DetectedPeak> &peaks, double xMin, double xMax);
   friend class QRootCanvas;
   friend class IntegralDialog;
   friend class DisplayParamsDialog;
   friend class EfficiencyDialog;
   friend class AutoCalibDialog;
   friend class MacroDialog;

public:
    // Block 4: Command Strings / Macros (Dn, Cn, Mn, Zn, n)
    struct MacroDefinition {
        int id{0};
        QString commandString;
        QString spectrumListFile;
        int cycles{1};
        QString description;
    };
    void openMacroDialog();
    bool executeMacroCommand(const QString &commandToken);
    void executeCommandString(const QString &cmdStr);
    const std::map<int, MacroDefinition>& getMacros() const { return m_macros; }
    void setMacro(int macroId, const MacroDefinition &def) { m_macros[macroId] = def; }
    enum class LoadBehavior {
        Autoscale,
        PreserveScale
    };

   QMainCanvas( QWidget *parent = 0);
   virtual ~QMainCanvas();
   virtual void changeEvent(QEvent * e);
   virtual void closeEvent(QCloseEvent *e);
   virtual void keyPressEvent(QKeyEvent *event);
   virtual void keyReleaseEvent(QKeyEvent *event);
   bool eventFilter(QObject *watched, QEvent *event) override;
   int getBinFromClick(int x, int y);
   Double_t findMinValueInInterval(int, int);
   Double_t findMaxValueInInterval(int, int);

   enum class AreaLogContext { None, Integration, Fitting };

   bool startAreaLogging(const QString& fileName);
   bool stopAreaLogging();
   bool isAreaLoggingEnabled() const { return m_isAreaLoggingEnabled; }
   QString getAreaLogFileName() const { return m_areaLogFile.fileName(); }
   void writeAreaLogHeader(bool isFitting);
   void writeAreaLogData(double centroid, double fwhm, double gross, double net, double background, double error);

   // Block 3: Setup & Parameter Definition Dialogs
   void openDisplayParamsDialog();
   void openEfficiencyDialog();
   void openPeakWidthModeDialog();
   void openAutoCalibDialog();
   const EfficiencyConfig& getEfficiencyConfig() const { return m_efficiencyConfig; }
   void setEfficiencyConfig(const EfficiencyConfig &cfg) { m_efficiencyConfig = cfg; }
   double evaluateEfficiency(double energyKeV) const;
         int numberoftimes=1;
   std::vector<int> colors_hist={4,2,3,7,6,1,5,28,38,30,8};

   //The histogram which is declared globally so every function can access it
   TracknHistogram *h1f;
   //These are some global variables for the integral function which are the parameters for the best fitted line of the background
   Double_t slope=0,addition=0;
   int SelectedElement_i=1, SelectedElement_j=1; 
   int PilgrimElement_i=1, PilgrimElement_j=1;
   int maxElement_i=1, maxElement_j=1;
   Double_t mousePilgrimX, mousePilgrimY, mouseLeftClickXcoord, mouseLeftClickYcoord;
   TH1F* HijF[12][12];
   std::vector<TH1F*> HijC[12][12];
   TH1F* selectedHisto;
   int maxk=0;
   int maxkk=0;

public slots:
   void clicked1();
   void clickedW();
   void areaFunction();
   void areaFunctionWithBackground(bool openDialog = true);
   void handle_root_events();
   void autoFit(int, int);
   void clearTheScreen();
   void addBackgroundMarker(Int_t, Int_t);
   void addIntegralMarker(Int_t, Int_t);
   void deleteBackgroundMarkers();
   void deleteIntegralMarkers();
   void deleteAllMarkers();
   void showBackgroundMarkers();
   void showIntegralMarkers();
   void showAllMarkers();
   void showMJMarkers();
   void showMVMarkers();
   void deleteZJMarkers();
   void deleteZVMarkers();
   void drawZeroLine();
   void deleteNearestGaussMarker(Int_t, Int_t);
   void quickEnergyCalibration();
   void showMatrixProjection();
   void addRangeMarker(Int_t, Int_t);
   void deleteRangeMarkers();
   void showRangeMarkers();
   void addGaussMarker(Int_t, Int_t);
   void deleteGaussMarkers();
   void showGaussMarkers();
    void addGateMarker(Int_t, Int_t);
    void deleteGateMarkers();
    void showGateMarkers();
    void fitGauss();
    void searchPeaks();
    void searchPeaksWithParams(double sigma, double threshold);
    void deletePeakMarkers();
    void showPeakMarkers();
    void renderPeakSearchLabels(int z, int g);
    void transferPeaksToGaussMarkers();
    void Cal2pMain();
   void zoomTheScreen();
   void goToEnergy();
   void zoomAroundCursor(Int_t x, Int_t y);
   void autoIntegrationAtCursor(Int_t x, Int_t y);
   void translateplusTheScreen();
   void translateminusTheScreen();
   void shiftDisplayLeft75();
   void shiftDisplayRight75();
   void translatedownTheScreen();
   void translateupTheScreen();
   void zoomOut();
    void fullX(bool logPrompt = true);
    void fullY(bool logPrompt = true);
    void sameX();
    void sameY();
   void addSpaceBarMarker(Int_t, Int_t);
   void offerHelp();
   void AddCulomn();
   void AddLine();
   void IdentifyLastClickedHistogram(Double_t z, Double_t y);
   void IdentifyLastPilgrimHistogram(Double_t x, Double_t y);
   void ColorTheFrameOfTheHistogram();
   void OpenColorSelectionDialog();
   void openAllDialogsForInspection();
   void showXYcoord(Double_t, Double_t);
   void DeleteCulomn();
   void DeleteLine();
    void RefreshScreen();
    //void DrawHisto();
    void toggleLogY();
    void updateAxisStatusLabels();
    void adjustYAxisToVisibleMax(TH1F *hist, int z = -1, int g = -1);
    void renderPeakLabels(int z, int g);
    QSplitter* getMainSplitter() const { return mainSplitter; }

    // Axis range adjustment slots (Table 2)
    void adjustAxisRange(const QString &axisName, bool increase, bool fineStep);

    // Multi-spectrum navigation slots (# - and # +) (Table 3)
    void onSpectrumIncrement();
    void onSpectrumDecrement();
    void onSpectrumIncrementSameScale();
    void onSpectrumDecrementSameScale();
    void stepSpectrumIndex(int delta, bool preserveScale = false);

    // Block 4: Command Strings / Macros Slots (Dn, Cn, Mn, Zn, n)
    void defineMacro(int macroId);
    void executeMacro(int macroId);
    void cycleMacro(int macroId, int cycles = -1);
    void showMacro(int macroId);
    void clearMacro(int macroId);

    // Energy calibration dialogs
    void openEnCalDialog();
    void openTrackFitDialog();
    void executeATCalibration();
    void openIntegralDialog();
    void closeIntegralDialog();
    void onDirectAutoTrace();
    
    // File & I/O methods
    void printPlot();
    void setYMax(double yVal);
    void setYMin(double yVal);
    void setLoadBehavior(LoadBehavior behavior) { m_loadBehavior = behavior; }

    // GASPware Compressed Matrix slots
    void onOpenCMClicked();
    void onGateCMClicked();
    void loadSpectrumDataToPad(const std::vector<double> &data, const QString &title, bool asOverlay = false);

    TracknHistogram* getActiveTracknHistogram() const {
        if (SelectedElement_i >= 1 && SelectedElement_i <= maxElement_i &&
            SelectedElement_j >= 1 && SelectedElement_j <= maxElement_j) {
            return dynamic_cast<TracknHistogram*>(HijF[SelectedElement_i][SelectedElement_j]);
        }
        return nullptr;
    }
    const std::vector<Float_t>& getPuncteCalib2p() const { return puncte_calib2p; }
    const std::vector<Double_t>& getSpacebarMarkers() const { return spacebar_markers; }
    const std::vector<Double_t>& getGateMarkers() const { return gate_markers; }
    const std::vector<Double_t>& getGaussCenters(int i, int j) const { return gaussCenters[i][j]; }
    int getCurrentSpectrumIndex() const { return m_currentSpectrumIndex; }
    QRootCanvas* getRootCanvas() const { return canvas; }

protected:
    //virtual void paintEvent(QPaintEvent *event);
    void clearDrawnObjects();

    QSplitter      *mainSplitter = nullptr;
    QRootCanvas    *canvas = nullptr;
    QPushButton    *b;
    QTimer         *fRootTimer;
    TList listOfObjectsDrawnOnScreen;
    std::vector<Int_t> integral_markers;
    std::vector<Int_t> background_markers;
    std::vector<Double_t> spacebar_markers;
    std::vector<Double_t> range_markers;
    std::vector<Double_t> gauss_markers;
    std::vector<Double_t> zoom_markers;
    std::vector<Double_t> gate_markers;
   std::vector<TF1> backgroundFunctionVector;
   std::vector<TF1> gaussianWithBackgroundFunctionVector;
   std::vector<TObject*> autoFitMarkers[12][12];
   std::vector<Double_t> gaussCenters[12][12];
   std::vector<Double_t> gaussCentersHeight[12][12];
    std::vector<Double_t> peakSearchCenters[12][12];
    std::vector<Double_t> peakSearchHeights[12][12];
    std::vector<TObject*> peakSearchPrimitives[12][12];
    TList autoFitLatex[12][12];
   std::vector<std::string> latexTexts;
   
   Float_t maxValueInHistogram;
   std::vector<Float_t> puncte_calib2p;
   double_t backgroundA0, backgroundA1;
   double_t backgroundIntegral, backgroundIntegralError;
   TMatrixD *backgroundCovarianceMatrix = nullptr;
   TLine* lineR = nullptr;
   TLine* lineL = nullptr;
   TLine* lineD = nullptr;
   TLine* lineU = nullptr;
      //Tline *backgroundLine;
      QLabel *labelX = nullptr;
      QLabel *labelY = nullptr;
      
      IntegralDialog *m_integralDialog = nullptr;

      // Xtrackn top status header labels
      QLabel *labelXMin = nullptr;
      QLabel *labelXMax = nullptr;
      QLabel *labelYMin = nullptr;
      QLabel *labelYMax = nullptr;
      QLabel *labelChannel = nullptr;
      QLabel *labelEnergy = nullptr;
      QLabel *labelCounts = nullptr;
      QLabel *labelCursorY = nullptr;

      // Xtrackn bottom control bar status labels
      QLabel *labelSpectrumFile = nullptr;
      QLabel *labelWorkingPath = nullptr;
      QLabel *labelOutputFile = nullptr;
    TFormula *gaussianWithBackground = nullptr;
    TF1 *gaussianWithBackgroundFunction = nullptr;
    //TLine *backgroundLine1;
    //TLine *backgroundLine2;


    TFormula *background = nullptr; 
    TF1 *backgroundFunction = nullptr;
    TLatex *gaussianCenterMarkerText = nullptr;

    // Unfocused fit parameters dialog and interactive background controls
    QDialog *fitParamsDialog = nullptr;
    QTextBrowser *fitParamsBrowser = nullptr;
    QCheckBox *chkUncoupleWidths = nullptr;
    QCheckBox *chkFixBackground = nullptr;
    QDoubleSpinBox *spinBkgSlope = nullptr;
    QDoubleSpinBox *spinBkgIntercept = nullptr;
    QTimer *bkgDebounceTimer = nullptr;
    bool m_uncoupleWidths{false};
    bool m_bkgFixed{false};
    double m_bkgSlopeVal{0.0};
    double m_bkgInterceptVal{0.0};
    int m_lastFitType{2}; // 1 = AutoFit, 2 = MultiPeakFit
    int m_lastAutoFitX{0};
    int m_lastAutoFitY{0};
    bool m_isRefitting{false};
    std::size_t m_lastMultiPeakCount{0};
    TLine *multiPeakBkgLine = nullptr;

    // Unfocused peak search parameters dialog
    QDialog *peakSearchParamsDialog = nullptr;
    QDoubleSpinBox *spinPeakSigma = nullptr;
    QDoubleSpinBox *spinPeakThreshold = nullptr;
    QTextBrowser *peakSearchBrowser = nullptr;
    QTimer *peakSearchDebounceTimer = nullptr;
    double m_peakSearchSigma{2.5};
    double m_peakSearchThreshold{0.05};

    // Interactive fitted peak controls
    QScrollArea *peaksScrollArea = nullptr;
    QWidget *peaksContainer = nullptr;
    QVBoxLayout *peaksLayout = nullptr;
    std::vector<PeakUIControls> m_peakUIControls;
    std::vector<PeakParamState> m_peakFixedStates;

    // Multi-spectrum file state
    LoadBehavior   m_loadBehavior{LoadBehavior::Autoscale};
    
    QString        m_currentSpectrumFile;
    QString        m_currentOutputFile;
    int            m_currentSpectrumIndex{0};
    int            m_currentSpectrumCount{1};
    int            m_currentSpectrumLength{10240};
    SpectrumFormat m_currentSpectrumFormat{SpectrumFormat::LongInt32};

    // Interactive buttons for modifier event filtering (Table 3)
    QPushButton                   *btnDT{nullptr};
    QPushButton                   *btnInc{nullptr};
    QPushButton                   *btnDec{nullptr};
    QPushButton                   *btnOpenCM{nullptr};
    QPushButton                   *btnGateCM{nullptr};
    std::shared_ptr<MatrixReader>  m_currentMatrix;

    // Area Output Logging
    bool            m_isAreaLoggingEnabled{false};
    AreaLogContext  m_lastAreaLogContext{AreaLogContext::None};
    QFile           m_areaLogFile;
    QTextStream     m_areaLogStream;

    // Block 3: Display Parameters (DD) & Efficiency (DE)
    double          m_autoscaleHeadroomLinear{10.0};
    double          m_autoscaleHeadroomLog{30.0};
    int             m_defaultZoomWidth{200};
    int             m_gridDivisionsX{10};
    int             m_gridDivisionsY{10};
    int             m_gridLineWidth{1};
    int             m_gridLineStyle{2}; // 1 = Solid, 2 = Dashed, 3 = Dotted
    EfficiencyConfig m_efficiencyConfig;

    // Block 4: Macros
    std::map<int, MacroDefinition> m_macros;

    // Interactive In-App Help
    HelpDialog      *m_helpDialog{nullptr};
};


#endif

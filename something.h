#ifndef SOMETHING_H
#define SOMETHING_H

#include <QMainWindow>
#include <QTableView>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QProgressBar>
#include <QPushButton>
#include <QLineEdit>
#include "USNParser.h"
#include "Monitor.h"
#include "FileIndex.h"
#include <windows.h>
#include <QCloseEvent>
#include <QListWidget.h>
#include <QLabel>
#include "History.h"
#include "Searcher.h"
#include "HTMLDelegate.h"

namespace Ui {
class Something;
}

class Something : public QMainWindow
{
  Q_OBJECT

public:
  explicit Something(QWidget *parent = 0);
  ~Something();
  void initEngine();
  void keyPressEvent(QKeyEvent  *event) override;
  void mousePressEvent(QMouseEvent* event) override;
protected:
  void closeEvent(QCloseEvent *e);
private slots:
  void search();
  void recvPUSN(int id, PUSN_RECORD pusn);
  void buildIndexSlot();
  void updateResult();
  void showRecommend(const QString& path);
  void click_rec(QListWidgetItem* item);
  void unlock() { searcher->lock.unlock(); }
private:
  Ui::Something *ui;
  QLineEdit* input;
  QTableView* table;
  QListWidget* list;
  QStandardItemModel* model;
  HTMLDelegate* delegate;
  Searcher* searcher;
  QMenu* menu;
  QAction* buildIndex;
  QLabel* pLabel;
  QProgressBar* pProgressBar;
  History history;
  void createUI();
};
Q_DECLARE_METATYPE(PUSN_RECORD);
#endif // SOMETHING_H

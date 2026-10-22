#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTabBar>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <string>
#include <vector>

struct CommandItem {
    QString id;
    QString title;
    QString subtitle;
    QString category;
    QString statusLabel;
    QString previewTitle;
    QString previewPath;
    QString kind;
    QString contents;
    QString lastOpened;
    QString primaryAction;
    QString iconText;
};

static const std::vector<CommandItem> kItems = {
    {"open-downloads", "Open Downloads", "Jump to your downloaded files", "Commands", "↵", "Downloads", "/Users/you/Downloads", "Folder", "24 items", "Today, 10:42", "Open in Finder", "▣"},
    {"quick-note", "Create a quick note", "Capture a thought in a new document", "Commands", "⌘ N", "Quick Note", "/Users/you/Documents/Notes", "Command", "Markdown Template", "Today, 09:15", "Create Note", "✎"},
    {"toggle-dnd", "Toggle Do Not Disturb", "Silence notifications and stay focused", "Commands", "System", "Do Not Disturb", "System Settings / Focus", "System Toggle", "Status: Off", "Yesterday, 22:00", "Toggle Focus", "☾"},
    {"weekly-review", "Weekly review.md", "Documents / Notes", "Files", "12m ago", "Weekly review.md", "/Users/you/Documents/Notes/Weekly review.md", "Markdown Document", "4.2 KB", "12m ago", "Open Document", "≡"},
    {"interface-exploration", "Interface exploration.fig", "Documents / Design", "Files", "38m ago", "Interface exploration.fig", "/Users/you/Documents/Design/Interface exploration.fig", "Figma Design File", "18.7 MB", "38m ago", "Open in Figma", "◌"},
    {"terminal", "Terminal", "Applications / Utilities", "Applications", "1h ago", "Terminal.app", "/System/Applications/Utilities/Terminal.app", "Application", "Version 2.14", "1h ago", "Launch Application", "⌘"},
};

class ResultListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit ResultListWidget(QWidget* parent = nullptr) : QListWidget(parent) {
        setFrameShape(QFrame::NoFrame);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setAlternatingRowColors(false);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setStyleSheet(R"(
            QListWidget {
                background: transparent;
                color: #e3e3e3;
                border: none;
            }
            QListWidget::item {
                border-radius: 12px;
                padding: 8px 10px;
                margin: 2px 0;
            }
            QListWidget::item:selected {
                background: #ededed;
                color: #141414;
            }
        )");
    }

    void setEntries(const std::vector<CommandItem>& items) {
        clear();
        for (const auto& item : items) {
            auto* widget = new QWidget(this);
            auto* layout = new QHBoxLayout(widget);
            layout->setContentsMargins(10, 8, 10, 8);
            layout->setSpacing(12);

            auto* icon = new QLabel(item.iconText, widget);
            icon->setStyleSheet("QLabel { font-size: 16px; color: #8c8c8c; }");
            icon->setFixedWidth(18);

            auto* textLayout = new QVBoxLayout();
            textLayout->setContentsMargins(0, 0, 0, 0);
            textLayout->setSpacing(2);

            auto* title = new QLabel(item.title, widget);
            title->setStyleSheet("QLabel { font-size: 14px; font-weight: 500; color: #ededed; }");

            auto* subtitle = new QLabel(item.subtitle, widget);
            subtitle->setStyleSheet("QLabel { font-size: 12px; color: #787878; }");

            textLayout->addWidget(title);
            textLayout->addWidget(subtitle);
            layout->addWidget(icon);
            layout->addLayout(textLayout);
            layout->addStretch();

            auto* badge = new QLabel(item.statusLabel, widget);
            badge->setStyleSheet("QLabel { font-size: 12px; color: #737373; padding: 4px 6px; border-radius: 5px; border: 1px solid #363636; background: #262626; }");
            if (item.category == "Commands" && item.statusLabel == "↵") {
                badge->setStyleSheet("QLabel { font-size: 11px; color: #8a8a8a; padding: 3px 6px; border-radius: 5px; border: 1px solid #363636; background: #262626; }");
            }
            layout->addWidget(badge);

            auto* itemWidget = new QListWidgetItem(this);
            itemWidget->setSizeHint(widget->sizeHint());
            setItemWidget(itemWidget, widget);
            itemWidget->setData(Qt::UserRole, item.id);
        }
    }
};

class PreviewPanel : public QWidget {
    Q_OBJECT
public:
    explicit PreviewPanel(QWidget* parent = nullptr) : QWidget(parent) {
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(18, 18, 18, 18);
        layout->setSpacing(12);

        auto* header = new QHBoxLayout();
        auto* label = new QLabel("Preview");
        label->setStyleSheet("QLabel { color: #787878; font-size: 12px; }");

        auto* toggle = new QPushButton("▣");
        toggle->setStyleSheet(R"(
            QPushButton {
                color: #888888;
                background: transparent;
                border: none;
                font-size: 14px;
            }
        )");
        header->addWidget(label);
        header->addStretch();
        header->addWidget(toggle);
        layout->addLayout(header);

        iconBox_ = new QLabel("◌");
        iconBox_->setAlignment(Qt::AlignCenter);
        iconBox_->setStyleSheet("QLabel { background: #242424; border: 1px solid #303030; border-radius: 18px; min-width: 72px; min-height: 72px; font-size: 28px; color: #d4d4d4; }");
        layout->addWidget(iconBox_);

        title_ = new QLabel("Downloads");
        title_->setStyleSheet("QLabel { color: #f5f5f5; font-size: 20px; font-weight: 600; }");
        layout->addWidget(title_);

        path_ = new QLabel("/Users/you/Downloads");
        path_->setStyleSheet("QLabel { color: #737373; font-size: 12px; }");
        layout->addWidget(path_);

        metaWidget_ = new QWidget(this);
        auto* metaLayout = new QVBoxLayout(metaWidget_);
        metaLayout->setContentsMargins(0, 0, 0, 0);
        metaLayout->setSpacing(10);

        for (int i = 0; i < 3; ++i) {
            auto* row = new QWidget(this);
            auto* rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            auto* name = new QLabel();
            name->setStyleSheet("QLabel { color: #737373; font-size: 12px; }");
            auto* value = new QLabel();
            value->setStyleSheet("QLabel { color: #d4d4d4; font-weight: 500; font-size: 12px; }");
            rowLayout->addWidget(name);
            rowLayout->addStretch();
            rowLayout->addWidget(value);
            metaRows_.push_back({name, value});
            metaLayout->addWidget(row);
        }
        layout->addWidget(metaWidget_);

        auto* actions = new QVBoxLayout();
        actions->setSpacing(10);
        mainAction_ = new QPushButton("Open in Finder");
        mainAction_->setStyleSheet(R"(
            QPushButton {
                background: #2a2a2a;
                color: white;
                border: none;
                border-radius: 10px;
                padding: 10px 12px;
                text-align: left;
                font-size: 13px;
            }
        )");
        copyAction_ = new QPushButton("Copy path");
        copyAction_->setStyleSheet(R"(
            QPushButton {
                background: transparent;
                color: #8a8a8a;
                border: none;
                border-radius: 10px;
                padding: 8px 10px;
                text-align: left;
                font-size: 12px;
            }
        )");
        layout->addWidget(mainAction_);
        layout->addWidget(copyAction_);
    }

    void setItem(const CommandItem& item) {
        iconBox_->setText(item.iconText);
        title_->setText(item.previewTitle);
        path_->setText(item.previewPath);

        const std::vector<std::pair<QString, QString>> values = {
            {"Kind", item.kind},
            {"Contents", item.contents},
            {"Last opened", item.lastOpened},
        };

        for (size_t i = 0; i < metaRows_.size(); ++i) {
            metaRows_[i].first->setText(values[i].first);
            metaRows_[i].second->setText(values[i].second);
        }

        mainAction_->setText(item.primaryAction);
    }

private:
    QLabel* iconBox_;
    QLabel* title_;
    QLabel* path_;
    QWidget* metaWidget_;
    std::vector<std::pair<QLabel*, QLabel*>> metaRows_;
    QPushButton* mainAction_;
    QPushButton* copyAction_;
};

class CommandPaletteWindow : public QWidget {
    Q_OBJECT
public:
    CommandPaletteWindow(QWidget* parent = nullptr) : QWidget(parent) {
        setObjectName("CommandPaletteWindow");
        setWindowTitle("Command Palette");
        setMinimumSize(780, 530);
        resize(920, 580);
        setStyleSheet(R"(
            QWidget#CommandPaletteWindow {
                background: #111111;
                color: #f2f2f2;
            }
            QWidget {
                font-family: "Segoe UI", "SF Pro Display", sans-serif;
            }
        )");

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        auto* panel = new QWidget(this);
        panel->setStyleSheet(R"(
            QWidget {
                background: rgba(28,28,28,0.96);
                border: 1px solid #2a2a2a;
                border-radius: 20px;
            }
        )");

        auto* panelLayout = new QVBoxLayout(panel);
        panelLayout->setContentsMargins(0, 0, 0, 0);
        panelLayout->setSpacing(0);

        auto* searchRow = new QWidget(panel);
        searchRow->setStyleSheet("QWidget { background: transparent; }");
        auto* searchLayout = new QHBoxLayout(searchRow);
        searchLayout->setContentsMargins(18, 18, 18, 14);
        searchLayout->setSpacing(12);

        auto* icon = new QLabel("⌕", searchRow);
        icon->setStyleSheet("QLabel { color: #7a7a7a; font-size: 18px; }");
        searchField_ = new QLineEdit(searchRow);
        searchField_->setPlaceholderText("Search apps, files, and commands...");
        searchField_->setStyleSheet(R"(
            QLineEdit {
                background: transparent;
                border: none;
                color: #f1f1f1;
                font-size: 19px;
            }
            QLineEdit::placeholder {
                color: #737373;
            }
        )");
        auto* esc = new QLabel("esc", searchRow);
        esc->setStyleSheet("QLabel { background: #262626; border: 1px solid #363636; border-radius: 6px; padding: 4px 8px; color: #8a8a8a; font-size: 11px; }");
        searchLayout->addWidget(icon);
        searchLayout->addWidget(searchField_);
        searchLayout->addWidget(esc);
        panelLayout->addWidget(searchRow);

        auto* tabsRow = new QWidget(panel);
        auto* tabsLayout = new QHBoxLayout(tabsRow);
        tabsLayout->setContentsMargins(18, 0, 18, 14);
        tabsLayout->setSpacing(8);
        tabs_ = new QTabBar(tabsRow);
        tabs_->addTab("All");
        tabs_->addTab("Applications");
        tabs_->addTab("Files");
        tabs_->addTab("Commands");
        tabs_->setStyleSheet(R"(
            QTabBar::tab {
                background: transparent;
                color: #808080;
                border: none;
                padding: 7px 11px;
                margin-right: 6px;
                border-radius: 8px;
                font-size: 13px;
            }
            QTabBar::tab:selected {
                background: #2c2c2c;
                color: #f4f4f4;
            }
        )");
        tabs_->setCurrentIndex(0);
        auto* switchWidget = new QWidget(tabsRow);
        auto* switchLayout = new QHBoxLayout(switchWidget);
        switchLayout->setContentsMargins(0, 0, 0, 0);
        switchLayout->setSpacing(6);
        auto* switchText = new QLabel("Switch", switchWidget);
        switchText->setStyleSheet("QLabel { color: #737373; font-size: 12px; }");
        auto* switchKey = new QLabel("⇥", switchWidget);
        switchKey->setStyleSheet("QLabel { background: #262626; border: 1px solid #363636; border-radius: 5px; color: #9d9d9d; padding: 2px 4px; font-size: 12px; }");
        switchLayout->addWidget(switchText);
        switchLayout->addWidget(switchKey);
        tabsLayout->addWidget(tabs_);
        tabsLayout->addStretch();
        tabsLayout->addWidget(switchWidget);
        panelLayout->addWidget(tabsRow);

        auto* content = new QWidget(panel);
        auto* contentLayout = new QHBoxLayout(content);
        contentLayout->setContentsMargins(10, 10, 10, 0);
        contentLayout->setSpacing(0);

        listWidget_ = new ResultListWidget(content);
        listWidget_->setEntries(kItems);
        preview_ = new PreviewPanel(content);
        preview_->setItem(kItems.front());
        preview_->setFixedWidth(290);

        contentLayout->addWidget(listWidget_, 2);
        contentLayout->addWidget(preview_, 1);
        panelLayout->addWidget(content);

        auto* footer = new QWidget(panel);
        footer->setStyleSheet("QWidget { background: rgba(24,24,24,1); border-top: 1px solid rgba(255,255,255,0.05); }");
        auto* footerLayout = new QHBoxLayout(footer);
        footerLayout->setContentsMargins(18, 12, 18, 12);

        auto* footerLeft = new QWidget(footer);
        auto* leftLayout = new QHBoxLayout(footerLeft);
        leftLayout->setContentsMargins(0, 0, 0, 0);
        leftLayout->setSpacing(12);
        leftLayout->addWidget(keyLabel("↑"));
        leftLayout->addWidget(keyLabel("↓"));
        leftLayout->addWidget(new QLabel("Navigate"));
        leftLayout->addWidget(keyLabel("↵"));
        leftLayout->addWidget(new QLabel("Open"));

        auto* footerRight = new QWidget(footer);
        auto* rightLayout = new QHBoxLayout(footerRight);
        rightLayout->setContentsMargins(0, 0, 0, 0);
        rightLayout->setSpacing(10);
        rightLayout->addWidget(new QLabel("Actions"));
        rightLayout->addWidget(keyLabel("⌘"));
        rightLayout->addWidget(keyLabel("↵"));

        footerLayout->addWidget(footerLeft);
        footerLayout->addStretch();
        footerLayout->addWidget(footerRight);
        panelLayout->addWidget(footer);

        root->addWidget(panel);

        auto* caption = new QWidget(this);
        auto* captionLayout = new QHBoxLayout(caption);
        captionLayout->setContentsMargins(6, 12, 6, 0);
        captionLayout->addWidget(new QLabel("A little less clicking —"));
        captionLayout->addWidget(keyLabel("⌘ K"));
        captionLayout->addWidget(new QLabel("from anywhere."));
        captionLayout->addStretch();
        captionLayout->addWidget(new QLabel("Personal workspace"));
        root->addWidget(caption);

        searchField_->textChanged.connect([this](const QString& text) {
            const QString q = text.trimmed().toLower();
            std::vector<CommandItem> matches;
            for (const auto& item : kItems) {
                if (q.isEmpty() || item.title.toLower().contains(q) || item.subtitle.toLower().contains(q)) {
                    matches.push_back(item);
                }
            }
            listWidget_->setEntries(matches.empty() ? kItems : matches);
            if (!matches.empty()) {
                preview_->setItem(matches.front());
            }
        });

        QObject::connect(listWidget_, &QListWidget::currentItemChanged, [this](QListWidgetItem* current, QListWidgetItem*) {
            if (!current) return;
            const auto id = current->data(Qt::UserRole).toString();
            for (const auto& item : kItems) {
                if (item.id == id) {
                    preview_->setItem(item);
                    return;
                }
            }
        });

        tabs_->tabBarClicked.connect([this](int index) {
            const QString tabName = tabs_->tabText(index);
            std::vector<CommandItem> matches;
            for (const auto& item : kItems) {
                if (tabName == "All" || item.category == tabName) {
                    matches.push_back(item);
                }
            }
            listWidget_->setEntries(matches);
            if (!matches.empty()) preview_->setItem(matches.front());
        });

        listWidget_->setCurrentRow(0);
    }

private:
    static QLabel* keyLabel(const QString& text) {
        auto* label = new QLabel(text);
        label->setStyleSheet(R"(
            QLabel {
                background: #252525;
                border: 1px solid #343434;
                border-radius: 5px;
                color: #999999;
                padding: 2px 5px;
                font-size: 11px;
            }
        )");
        return label;
    }

    QLineEdit* searchField_;
    QTabBar* tabs_;
    ResultListWidget* listWidget_;
    PreviewPanel* preview_;
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    CommandPaletteWindow window;
    window.show();
    return app.exec();
}

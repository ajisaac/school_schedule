#include <QApplication>
#include <QColor>
#include <QCursor>
#include <QDate>
#include <QFile>
#include <QFileDialog>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTabWidget>
#include <QTextStream>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

// Allows me to see all assignments, lectures, labs, etc on a calendar
// And to merge them into a cohesive view
// Allows me to check off each one as I've done them

struct Assignment {
    QDate date;
    QString title;
    QString className;
    bool done = false;
};

// A link saved against a class: the text that shows in the list, and where it goes.
struct Resource {
    QString text;
    QString href;
};

// Dates in the file have no year, so anything without one lands in the current year.
static int resolveYear(const QString& captured) {
    if (captured.isEmpty()) {
        return QDate::currentDate().year();
    }
    const int year = captured.toInt();
    return year < 100 ? 2000 + year : year;
}

static QDate monthFromName(const QString& name, int day, int year) {
    for (const QString& format : {QStringLiteral("MMM"), QStringLiteral("MMMM")}) {
        const QDate parsed = QDate::fromString(name, format);
        if (parsed.isValid()) {
            return QDate(year, parsed.month(), day);
        }
    }
    return {};
}

// A file is a class name on its own line followed by that class's assignments,
// one per line, dated like "Sep 21 Read chapter 1" or "Aug 2 - Lab 3". Any later
// line that isn't a dated assignment starts a new class, so one file can also
// hold several classes back to back.
static QList<Assignment> parseAssignments(const QString& text) {
    static const QRegularExpression namedDate(
        R"(^([A-Za-z]{3,9})\.?\s+(\d{1,2})(?:st|nd|rd|th)?,?(?:\s+(\d{4}))?[\s:,\x{2014}\x{2013}-]+(.+)$)");
    static const QRegularExpression numericDate(
        R"(^(\d{1,2})[/-](\d{1,2})(?:[/-](\d{2,4}))?[\s:,\x{2014}\x{2013}-]+(.+)$)");

    QList<Assignment> assignments;
    QString className;

    const QStringList lines = text.split('\n');
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        QDate date;
        QString title;

        if (const auto match = namedDate.match(line); match.hasMatch()) {
            date = monthFromName(match.captured(1),
                                 match.captured(2).toInt(),
                                 resolveYear(match.captured(3)));
            title = match.captured(4).trimmed();
        } else if (const auto numeric = numericDate.match(line); numeric.hasMatch()) {
            date = QDate(resolveYear(numeric.captured(3)),
                         numeric.captured(1).toInt(),
                         numeric.captured(2).toInt());
            title = numeric.captured(4).trimmed();
        }

        if (date.isValid() && !title.isEmpty()) {
            assignments.append({date, title, className});
        } else {
            className = line;
        }
    }

    return assignments;
}

// Anything ticked off drops to grey, whatever class it belongs to.
static const QColor doneColor("#a0a0a0");

// Classes get a colour in the order they first show up, so a class keeps the
// same colour on the calendar, in the list and in the legend.
static QMap<QString, QColor> colorsForClasses(const QList<Assignment>& assignments) {
    static const QList<QColor> palette = {
        QColor("#1f77b4"), QColor("#d62728"), QColor("#2ca02c"), QColor("#ff7f0e"),
        QColor("#9467bd"), QColor("#17becf"), QColor("#8c564b"), QColor("#e377c2"),
    };

    QMap<QString, QColor> colors;
    int next = 0;
    for (const Assignment& assignment : assignments) {
        if (!colors.contains(assignment.className)) {
            colors.insert(assignment.className, palette.at(next % palette.size()));
            ++next;
        }
    }
    return colors;
}

// Asks for a resource's text and link together. Returns false if cancelled or
// if no link was given; otherwise fills in the resource.
static bool promptForResource(QWidget* parent, const QString& title, Resource* resource) {
    QDialog dialog(parent);
    dialog.setWindowTitle(title);

    auto* form = new QFormLayout(&dialog);
    auto* textField = new QLineEdit(resource->text, &dialog);
    auto* hrefField = new QLineEdit(resource->href, &dialog);
    form->addRow("Text:", textField);
    form->addRow("Link:", hrefField);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted || hrefField->text().trimmed().isEmpty()) {
        return false;
    }
    *resource = {textField->text().trimmed(), hrefField->text().trimmed()};
    return true;
}

// Everything that has ever been uploaded lives here, so the schedule is back
// the next time the program starts.
static const QString dataFilePath =
    QStringLiteral("/Users/aaron/program_data/school_schedule/data.json");

static bool saveData(const QList<Assignment>& assignments,
                     const QMap<QString, QList<Resource>>& resources,
                     QString* error) {
    const QFileInfo dataFileInfo(dataFilePath);
    if (!QDir().mkpath(dataFileInfo.absolutePath())) {
        *error = "Could not create " + dataFileInfo.absolutePath();
        return false;
    }

    QJsonArray array;
    for (const Assignment& assignment : assignments) {
        QJsonObject object;
        object["class"] = assignment.className;
        object["date"] = assignment.date.toString(Qt::ISODate);
        object["title"] = assignment.title;
        object["done"] = assignment.done;
        array.append(object);
    }

    QJsonObject resourcesObject;
    for (auto it = resources.constBegin(); it != resources.constEnd(); ++it) {
        QJsonArray classResources;
        for (const Resource& resource : it.value()) {
            QJsonObject object;
            object["text"] = resource.text;
            object["href"] = resource.href;
            classResources.append(object);
        }
        resourcesObject[it.key()] = classResources;
    }

    QJsonObject root;
    root["assignments"] = array;
    root["resources"] = resourcesObject;

    QFile file(dataFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        *error = "Could not write " + dataFilePath;
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

// A missing or unreadable file just means there is nothing saved yet.
static void loadSavedData(QList<Assignment>* assignments,
                          QMap<QString, QList<Resource>>* resources) {
    QFile file(dataFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();

    // Older files were a bare array of assignments with no resources.
    const QJsonObject root = document.object();
    const QJsonArray array =
        document.isArray() ? document.array() : root["assignments"].toArray();

    for (const QJsonValue& value : array) {
        const QJsonObject object = value.toObject();
        const QDate date = QDate::fromString(object["date"].toString(), Qt::ISODate);
        const QString title = object["title"].toString();
        if (date.isValid() && !title.isEmpty()) {
            assignments->append({date, title, object["class"].toString(),
                                 object["done"].toBool()});
        }
    }

    const QJsonObject resourcesObject = root["resources"].toObject();
    for (auto it = resourcesObject.constBegin(); it != resourcesObject.constEnd(); ++it) {
        const QJsonArray classResources = it.value().toArray();
        for (const QJsonValue& value : classResources) {
            const QJsonObject object = value.toObject();
            const QString href = object["href"].toString();
            if (!href.isEmpty()) {
                (*resources)[it.key()].append({object["text"].toString(), href});
            }
        }
    }
}

int main(int argc, char* argv[]) {
    QApplication a(argc, argv);

    QMainWindow window;
    window.setWindowTitle("School Schedule");
    window.resize(800, 600);

    auto* tabs = new QTabWidget(&window);
    window.setCentralWidget(tabs);

    // --- Assignments tab ---
    auto* assignmentsTab = new QWidget(tabs);
    auto* assignmentsLayout = new QVBoxLayout(assignmentsTab);

    auto* uploadButton = new QPushButton("Upload File...", assignmentsTab);
    auto* assignmentList = new QListWidget(assignmentsTab);

    assignmentsLayout->addWidget(uploadButton);
    assignmentsLayout->addWidget(assignmentList);
    tabs->addTab(assignmentsTab, "Assignments");

    // --- Classes tab ---
    auto* classesTab = new QWidget(tabs);
    auto* classesLayout = new QVBoxLayout(classesTab);

    auto* classTree = new QTreeWidget(classesTab);
    classTree->setHeaderHidden(true);
    classTree->setMouseTracking(true);
    auto* addResourceButton = new QPushButton("Add Resource...", classesTab);
    auto* editResourceButton = new QPushButton("Edit Resource...", classesTab);
    addResourceButton->setEnabled(false);
    editResourceButton->setEnabled(false);

    auto* resourceButtons = new QHBoxLayout();
    resourceButtons->addWidget(addResourceButton);
    resourceButtons->addWidget(editResourceButton);

    classesLayout->addWidget(classTree, 1);
    classesLayout->addLayout(resourceButtons);
    tabs->addTab(classesTab, "Classes");

    // --- Calendar tab ---
    auto* calendarTab = new QWidget(tabs);
    auto* calendarLayout = new QVBoxLayout(calendarTab);

    auto* navBar = new QHBoxLayout();
    auto* prevMonthButton = new QPushButton("<", calendarTab);
    auto* nextMonthButton = new QPushButton(">", calendarTab);
    auto* monthLabel = new QLabel(calendarTab);
    monthLabel->setAlignment(Qt::AlignCenter);

    navBar->addWidget(prevMonthButton);
    navBar->addWidget(monthLabel, 1);
    navBar->addWidget(nextMonthButton);
    calendarLayout->addLayout(navBar);

    auto* legendLabel = new QLabel(calendarTab);
    legendLabel->setAlignment(Qt::AlignCenter);
    legendLabel->setWordWrap(true);
    calendarLayout->addWidget(legendLabel);

    auto* monthGrid = new QGridLayout();
    calendarLayout->addLayout(monthGrid, 1);

    static const QStringList dayNames = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    for (int column = 0; column < 7; ++column) {
        auto* header = new QLabel(dayNames.at(column), calendarTab);
        header->setAlignment(Qt::AlignCenter);
        monthGrid->addWidget(header, 0, column);
    }

    // Six rows of seven cells covers any month layout; cells outside the month stay blank.
    QList<QLabel*> dayCells;
    for (int cell = 0; cell < 42; ++cell) {
        auto* dayCell = new QLabel(calendarTab);
        dayCell->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        dayCell->setFrameShape(QFrame::Box);
        dayCell->setWordWrap(true);
        dayCells.append(dayCell);
        monthGrid->addWidget(dayCell, 1 + cell / 7, cell % 7);
    }

    // Everything loaded so far, across every file that has been uploaded.
    QList<Assignment> allAssignments;
    QMap<QString, QColor> classColors;

    // Links saved against a class name, e.g. the syllabus or the course page.
    QMap<QString, QList<Resource>> resourcesByClass;

    loadSavedData(&allAssignments, &resourcesByClass);

    QDate shownMonth = QDate::currentDate();
    auto showMonth = [&shownMonth, &allAssignments, &classColors, monthLabel, dayCells]() {
        const QDate firstOfMonth(shownMonth.year(), shownMonth.month(), 1);
        monthLabel->setText(firstOfMonth.toString("MMMM yyyy"));

        // dayOfWeek() is 1 (Mon) to 7 (Sun); % 7 puts Sunday in the first column.
        const int startColumn = firstOfMonth.dayOfWeek() % 7;
        const int daysInMonth = firstOfMonth.daysInMonth();

        for (int cell = 0; cell < dayCells.size(); ++cell) {
            const int dayNumber = cell - startColumn + 1;
            if (dayNumber < 1 || dayNumber > daysInMonth) {
                dayCells.at(cell)->setText(QString());
                continue;
            }

            const QDate day(shownMonth.year(), shownMonth.month(), dayNumber);
            QString cellText = QString("<b>%1</b>").arg(dayNumber);
            for (const Assignment& assignment : allAssignments) {
                if (assignment.date == day) {
                    const QString title = assignment.done
                                              ? "<s>" + assignment.title.toHtmlEscaped() + "</s>"
                                              : assignment.title.toHtmlEscaped();
                    const QColor color = assignment.done
                                             ? doneColor
                                             : classColors.value(assignment.className);
                    cellText += QString(R"(<div style="color:%1">%2</div>)")
                                    .arg(color.name(), title);
                }
            }
            dayCells.at(cell)->setText(cellText);
        }
    };

    // Rows carry their class name, since the visible text also holds the
    // assignment counts. A resource row answers with the class it sits under.
    auto selectedClass = [classTree]() {
        const QTreeWidgetItem* item = classTree->currentItem();
        if (!item) {
            return QString();
        }
        if (item->parent()) {
            item = item->parent();
        }
        return item->data(0, Qt::UserRole).toString();
    };

    // Resource rows remember their spot in the class's list; -1 means the
    // selection isn't a resource.
    auto selectedResourceIndex = [classTree]() {
        const QTreeWidgetItem* item = classTree->currentItem();
        if (!item || !item->parent()) {
            return -1;
        }
        return item->data(0, Qt::UserRole + 2).toInt();
    };

    auto showClasses = [&allAssignments, &classColors, &resourcesByClass, classTree,
                        addResourceButton, editResourceButton, selectedClass,
                        selectedResourceIndex]() {
        QMap<QString, int> totalPerClass;
        QMap<QString, int> donePerClass;
        for (const Assignment& assignment : allAssignments) {
            ++totalPerClass[assignment.className];
            if (assignment.done) {
                ++donePerClass[assignment.className];
            }
        }

        const QString wasSelected = selectedClass();

        classTree->blockSignals(true);
        classTree->clear();
        for (auto it = classColors.constBegin(); it != classColors.constEnd(); ++it) {
            const int total = totalPerClass.value(it.key());
            const int completed = donePerClass.value(it.key());
            auto* classItem = new QTreeWidgetItem(classTree);
            classItem->setText(0, QString("%1  -  %2 assignments, %3 done")
                                      .arg(it.key())
                                      .arg(total)
                                      .arg(completed));
            classItem->setForeground(0, it.value());
            classItem->setData(0, Qt::UserRole, it.key());

            // The class's links hang underneath it.
            const QList<Resource> classResources = resourcesByClass.value(it.key());
            for (int index = 0; index < classResources.size(); ++index) {
                const Resource& resource = classResources.at(index);
                auto* resourceItem = new QTreeWidgetItem(classItem);
                resourceItem->setText(0, resource.text.isEmpty() ? resource.href
                                                                 : resource.text);
                resourceItem->setData(0, Qt::UserRole + 1, resource.href);
                resourceItem->setData(0, Qt::UserRole + 2, index);
                resourceItem->setToolTip(0, resource.href);
                resourceItem->setForeground(0, QColor("#1a5fb4"));

                QFont linkFont = resourceItem->font(0);
                linkFont.setUnderline(true);
                resourceItem->setFont(0, linkFont);
            }

            if (it.key() == wasSelected) {
                classTree->setCurrentItem(classItem);
            }
        }
        classTree->expandAll();
        classTree->blockSignals(false);

        addResourceButton->setEnabled(!selectedClass().isEmpty());
        editResourceButton->setEnabled(selectedResourceIndex() >= 0);
    };

    auto refresh = [&allAssignments, &classColors, assignmentList,
                    legendLabel, showMonth, showClasses]() {
        std::ranges::stable_sort(allAssignments,
                                 [](const Assignment& lhs, const Assignment& rhs) {
                                     return lhs.date < rhs.date;
                                 });
        classColors = colorsForClasses(allAssignments);

        // Refilling the list would otherwise fire itemChanged for every row.
        assignmentList->blockSignals(true);
        assignmentList->clear();
        for (int index = 0; index < allAssignments.size(); ++index) {
            const Assignment& assignment = allAssignments.at(index);
            auto* item = new QListWidgetItem(
                assignment.date.toString("MMM d, ddd") + "  -  " +
                assignment.className + "  -  " + assignment.title);
            item->setForeground(assignment.done ? doneColor
                                                : classColors.value(assignment.className));
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(assignment.done ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, index);

            QFont itemFont = item->font();
            itemFont.setStrikeOut(assignment.done);
            item->setFont(itemFont);

            assignmentList->addItem(item);
        }
        assignmentList->blockSignals(false);

        showClasses();

        QStringList legendParts;
        for (auto it = classColors.constBegin(); it != classColors.constEnd(); ++it) {
            legendParts.append(QString(R"(<span style="color:%1">&#9632; %2</span>)")
                                   .arg(it.value().name(), it.key().toHtmlEscaped()));
        }
        legendLabel->setText(legendParts.join("&nbsp;&nbsp;&nbsp;"));

        showMonth();
    };

    QObject::connect(classTree, &QTreeWidget::currentItemChanged,
                     [addResourceButton, editResourceButton, selectedClass,
                      selectedResourceIndex](QTreeWidgetItem*, QTreeWidgetItem*) {
        addResourceButton->setEnabled(!selectedClass().isEmpty());
        editResourceButton->setEnabled(selectedResourceIndex() >= 0);
    });

    // Resource rows get the browser's pointing hand; class rows and empty space
    // keep the normal arrow.
    QObject::connect(classTree, &QTreeWidget::itemEntered,
                     [classTree](QTreeWidgetItem* item, int) {
        const bool isLink = !item->data(0, Qt::UserRole + 1).toString().isEmpty();
        classTree->viewport()->setCursor(isLink ? Qt::PointingHandCursor : Qt::ArrowCursor);
    });
    QObject::connect(classTree, &QTreeWidget::viewportEntered, [classTree]() {
        classTree->viewport()->setCursor(Qt::ArrowCursor);
    });

    // Clicking a resource hands the link to the default browser.
    QObject::connect(classTree, &QTreeWidget::itemClicked, [](QTreeWidgetItem* item, int) {
        const QString href = item->data(0, Qt::UserRole + 1).toString();
        if (!href.isEmpty()) {
            QDesktopServices::openUrl(QUrl::fromUserInput(href));
        }
    });

    QObject::connect(addResourceButton, &QPushButton::clicked,
                     [&window, &allAssignments, &resourcesByClass, selectedClass,
                      showClasses]() {
        const QString className = selectedClass();
        if (className.isEmpty()) {
            return;
        }

        Resource resource;
        if (!promptForResource(&window, "Add Resource", &resource)) {
            return;
        }

        resourcesByClass[className].append(resource);
        showClasses();

        QString error;
        if (!saveData(allAssignments, resourcesByClass, &error)) {
            QMessageBox::warning(&window, "Save Failed", error);
        }
    });

    QObject::connect(editResourceButton, &QPushButton::clicked,
                     [&window, &allAssignments, &resourcesByClass, selectedClass,
                      selectedResourceIndex, showClasses]() {
        const QString className = selectedClass();
        const int index = selectedResourceIndex();
        if (className.isEmpty() || index < 0 ||
            index >= resourcesByClass.value(className).size()) {
            return;
        }

        Resource resource = resourcesByClass[className].at(index);
        if (!promptForResource(&window, "Edit Resource", &resource)) {
            return;
        }

        resourcesByClass[className][index] = resource;
        showClasses();

        QString error;
        if (!saveData(allAssignments, resourcesByClass, &error)) {
            QMessageBox::warning(&window, "Save Failed", error);
        }
    });

    // Ticking a box marks the assignment done and writes it straight back to disk.
    QObject::connect(assignmentList, &QListWidget::itemChanged,
                     [&window, &allAssignments, &resourcesByClass, &classColors,
                      showMonth, showClasses](QListWidgetItem* item) {
        const int index = item->data(Qt::UserRole).toInt();
        if (index < 0 || index >= allAssignments.size()) {
            return;
        }
        const Assignment& assignment = allAssignments[index];
        allAssignments[index].done = item->checkState() == Qt::Checked;

        QFont itemFont = item->font();
        itemFont.setStrikeOut(assignment.done);
        item->setFont(itemFont);
        item->setForeground(assignment.done ? doneColor
                                            : classColors.value(assignment.className));

        showMonth();
        showClasses();

        QString error;
        if (!saveData(allAssignments, resourcesByClass, &error)) {
            QMessageBox::warning(&window, "Save Failed", error);
        }
    });

    QObject::connect(prevMonthButton, &QPushButton::clicked, [&shownMonth, showMonth]() {
        shownMonth = shownMonth.addMonths(-1);
        showMonth();
    });
    QObject::connect(nextMonthButton, &QPushButton::clicked, [&shownMonth, showMonth]() {
        shownMonth = shownMonth.addMonths(1);
        showMonth();
    });

    QObject::connect(uploadButton, &QPushButton::clicked,
                     [&window, &allAssignments, &resourcesByClass, refresh]() {
        const QString path = QFileDialog::getOpenFileName(
            &window, "Open File", QString(), "All Files (*)");
        if (path.isEmpty()) {
            return;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(&window, "Upload Failed",
                                 "Could not open file:\n" + path);
            return;
        }

        QTextStream in(&file);
        QList<Assignment> loaded = parseAssignments(in.readAll());
        file.close();

        if (loaded.isEmpty()) {
            QMessageBox::information(&window, "No Assignments",
                                     "No dated assignments were found in:\n" + path);
            return;
        }

        // Re-uploading a class replaces what was already loaded for it rather
        // than doubling it up.
        QStringList loadedClasses;
        for (const Assignment& assignment : loaded) {
            if (!loadedClasses.contains(assignment.className)) {
                loadedClasses.append(assignment.className);
            }
        }
        // Re-uploading shouldn't clear the boxes already ticked, so carry the
        // done flag over to any assignment that comes back unchanged.
        QSet<QString> alreadyDone;
        for (const Assignment& assignment : allAssignments) {
            if (assignment.done) {
                alreadyDone.insert(assignment.className + '\n' +
                                   assignment.date.toString(Qt::ISODate) + '\n' +
                                   assignment.title);
            }
        }
        for (Assignment& assignment : loaded) {
            assignment.done = alreadyDone.contains(assignment.className + '\n' +
                                                   assignment.date.toString(Qt::ISODate) + '\n' +
                                                   assignment.title);
        }

        allAssignments.removeIf([&loadedClasses](const Assignment& assignment) {
            return loadedClasses.contains(assignment.className);
        });
        allAssignments.append(loaded);

        refresh();

        QString error;
        if (!saveData(allAssignments, resourcesByClass, &error)) {
            QMessageBox::warning(&window, "Save Failed", error);
        }

        window.setWindowTitle("School Schedule - " + QFileInfo(path).fileName());
    });

    refresh();
    tabs->addTab(calendarTab, "Calendar");
    tabs->setCurrentWidget(calendarTab);

    window.show();
    return QApplication::exec();
}

/****************************************************************************
** Meta object code from reading C++ file 'canvas.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "canvas.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'canvas.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_QRootCanvas_t {
    QByteArrayData data[19];
    char stringdata0[417];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_QRootCanvas_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_QRootCanvas_t qt_meta_stringdata_QRootCanvas = {
    {
QT_MOC_LITERAL(0, 0, 11), // "QRootCanvas"
QT_MOC_LITERAL(1, 12, 30), // "requestIntegrationNoBackground"
QT_MOC_LITERAL(2, 43, 0), // ""
QT_MOC_LITERAL(3, 44, 32), // "requestIntegrationWithBackground"
QT_MOC_LITERAL(4, 77, 16), // "autoFitRequested"
QT_MOC_LITERAL(5, 94, 21), // "requestClearTheScreen"
QT_MOC_LITERAL(6, 116, 28), // "addBackgroundMarkerRequested"
QT_MOC_LITERAL(7, 145, 5), // "Int_t"
QT_MOC_LITERAL(8, 151, 30), // "requestDeleteBackgroundMarkers"
QT_MOC_LITERAL(9, 182, 23), // "requestDeleteAllMarkers"
QT_MOC_LITERAL(10, 206, 28), // "requestShowBackgroundMarkers"
QT_MOC_LITERAL(11, 235, 21), // "requestShowAllMarkers"
QT_MOC_LITERAL(12, 257, 21), // "requestAddRangeMarker"
QT_MOC_LITERAL(13, 279, 25), // "requestDeleteRangeMarkers"
QT_MOC_LITERAL(14, 305, 23), // "requestShowRangeMarkers"
QT_MOC_LITERAL(15, 329, 21), // "requestAddGaussMarker"
QT_MOC_LITERAL(16, 351, 25), // "requestDeleteGaussMarkers"
QT_MOC_LITERAL(17, 377, 23), // "requestShowGaussMarkers"
QT_MOC_LITERAL(18, 401, 15) // "requestFitGauss"

    },
    "QRootCanvas\0requestIntegrationNoBackground\0"
    "\0requestIntegrationWithBackground\0"
    "autoFitRequested\0requestClearTheScreen\0"
    "addBackgroundMarkerRequested\0Int_t\0"
    "requestDeleteBackgroundMarkers\0"
    "requestDeleteAllMarkers\0"
    "requestShowBackgroundMarkers\0"
    "requestShowAllMarkers\0requestAddRangeMarker\0"
    "requestDeleteRangeMarkers\0"
    "requestShowRangeMarkers\0requestAddGaussMarker\0"
    "requestDeleteGaussMarkers\0"
    "requestShowGaussMarkers\0requestFitGauss"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_QRootCanvas[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      16,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
      16,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   94,    2, 0x06 /* Public */,
       3,    0,   95,    2, 0x06 /* Public */,
       4,    2,   96,    2, 0x06 /* Public */,
       5,    0,  101,    2, 0x06 /* Public */,
       6,    2,  102,    2, 0x06 /* Public */,
       8,    0,  107,    2, 0x06 /* Public */,
       9,    0,  108,    2, 0x06 /* Public */,
      10,    0,  109,    2, 0x06 /* Public */,
      11,    0,  110,    2, 0x06 /* Public */,
      12,    2,  111,    2, 0x06 /* Public */,
      13,    0,  116,    2, 0x06 /* Public */,
      14,    0,  117,    2, 0x06 /* Public */,
      15,    2,  118,    2, 0x06 /* Public */,
      16,    0,  123,    2, 0x06 /* Public */,
      17,    0,  124,    2, 0x06 /* Public */,
      18,    0,  125,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 7, 0x80000000 | 7,    2,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 7, 0x80000000 | 7,    2,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 7, 0x80000000 | 7,    2,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void QRootCanvas::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<QRootCanvas *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->requestIntegrationNoBackground(); break;
        case 1: _t->requestIntegrationWithBackground(); break;
        case 2: _t->autoFitRequested((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 3: _t->requestClearTheScreen(); break;
        case 4: _t->addBackgroundMarkerRequested((*reinterpret_cast< Int_t(*)>(_a[1])),(*reinterpret_cast< Int_t(*)>(_a[2]))); break;
        case 5: _t->requestDeleteBackgroundMarkers(); break;
        case 6: _t->requestDeleteAllMarkers(); break;
        case 7: _t->requestShowBackgroundMarkers(); break;
        case 8: _t->requestShowAllMarkers(); break;
        case 9: _t->requestAddRangeMarker((*reinterpret_cast< Int_t(*)>(_a[1])),(*reinterpret_cast< Int_t(*)>(_a[2]))); break;
        case 10: _t->requestDeleteRangeMarkers(); break;
        case 11: _t->requestShowRangeMarkers(); break;
        case 12: _t->requestAddGaussMarker((*reinterpret_cast< Int_t(*)>(_a[1])),(*reinterpret_cast< Int_t(*)>(_a[2]))); break;
        case 13: _t->requestDeleteGaussMarkers(); break;
        case 14: _t->requestShowGaussMarkers(); break;
        case 15: _t->requestFitGauss(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestIntegrationNoBackground)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestIntegrationWithBackground)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)(int , int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::autoFitRequested)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestClearTheScreen)) {
                *result = 3;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)(Int_t , Int_t );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::addBackgroundMarkerRequested)) {
                *result = 4;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestDeleteBackgroundMarkers)) {
                *result = 5;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestDeleteAllMarkers)) {
                *result = 6;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestShowBackgroundMarkers)) {
                *result = 7;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestShowAllMarkers)) {
                *result = 8;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)(Int_t , Int_t );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestAddRangeMarker)) {
                *result = 9;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestDeleteRangeMarkers)) {
                *result = 10;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestShowRangeMarkers)) {
                *result = 11;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)(Int_t , Int_t );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestAddGaussMarker)) {
                *result = 12;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestDeleteGaussMarkers)) {
                *result = 13;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestShowGaussMarkers)) {
                *result = 14;
                return;
            }
        }
        {
            using _t = void (QRootCanvas::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&QRootCanvas::requestFitGauss)) {
                *result = 15;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject QRootCanvas::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_QRootCanvas.data,
    qt_meta_data_QRootCanvas,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *QRootCanvas::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QRootCanvas::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_QRootCanvas.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QRootCanvas::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 16;
    }
    return _id;
}

// SIGNAL 0
void QRootCanvas::requestIntegrationNoBackground()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void QRootCanvas::requestIntegrationWithBackground()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void QRootCanvas::autoFitRequested(int _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QRootCanvas::requestClearTheScreen()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void QRootCanvas::addBackgroundMarkerRequested(Int_t _t1, Int_t _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void QRootCanvas::requestDeleteBackgroundMarkers()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void QRootCanvas::requestDeleteAllMarkers()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void QRootCanvas::requestShowBackgroundMarkers()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void QRootCanvas::requestShowAllMarkers()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void QRootCanvas::requestAddRangeMarker(Int_t _t1, Int_t _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 9, _a);
}

// SIGNAL 10
void QRootCanvas::requestDeleteRangeMarkers()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void QRootCanvas::requestShowRangeMarkers()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void QRootCanvas::requestAddGaussMarker(Int_t _t1, Int_t _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 12, _a);
}

// SIGNAL 13
void QRootCanvas::requestDeleteGaussMarkers()
{
    QMetaObject::activate(this, &staticMetaObject, 13, nullptr);
}

// SIGNAL 14
void QRootCanvas::requestShowGaussMarkers()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void QRootCanvas::requestFitGauss()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}
struct qt_meta_stringdata_QMainCanvas_t {
    QByteArrayData data[18];
    char stringdata0[276];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_QMainCanvas_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_QMainCanvas_t qt_meta_stringdata_QMainCanvas = {
    {
QT_MOC_LITERAL(0, 0, 11), // "QMainCanvas"
QT_MOC_LITERAL(1, 12, 8), // "clicked1"
QT_MOC_LITERAL(2, 21, 0), // ""
QT_MOC_LITERAL(3, 22, 12), // "areaFunction"
QT_MOC_LITERAL(4, 35, 26), // "areaFunctionWithBackground"
QT_MOC_LITERAL(5, 62, 18), // "handle_root_events"
QT_MOC_LITERAL(6, 81, 7), // "autoFit"
QT_MOC_LITERAL(7, 89, 14), // "clearTheScreen"
QT_MOC_LITERAL(8, 104, 19), // "addBackgroundMarker"
QT_MOC_LITERAL(9, 124, 5), // "Int_t"
QT_MOC_LITERAL(10, 130, 23), // "deleteBackgroundMarkers"
QT_MOC_LITERAL(11, 154, 16), // "deleteAllMarkers"
QT_MOC_LITERAL(12, 171, 21), // "showBackgroundMarkers"
QT_MOC_LITERAL(13, 193, 14), // "showAllMarkers"
QT_MOC_LITERAL(14, 208, 14), // "addRangeMarker"
QT_MOC_LITERAL(15, 223, 18), // "deleteRangeMarkers"
QT_MOC_LITERAL(16, 242, 16), // "showRangeMarkers"
QT_MOC_LITERAL(17, 259, 16) // "FindPeakFunction"

    },
    "QMainCanvas\0clicked1\0\0areaFunction\0"
    "areaFunctionWithBackground\0"
    "handle_root_events\0autoFit\0clearTheScreen\0"
    "addBackgroundMarker\0Int_t\0"
    "deleteBackgroundMarkers\0deleteAllMarkers\0"
    "showBackgroundMarkers\0showAllMarkers\0"
    "addRangeMarker\0deleteRangeMarkers\0"
    "showRangeMarkers\0FindPeakFunction"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_QMainCanvas[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      15,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   89,    2, 0x0a /* Public */,
       3,    0,   90,    2, 0x0a /* Public */,
       4,    0,   91,    2, 0x0a /* Public */,
       5,    0,   92,    2, 0x0a /* Public */,
       6,    2,   93,    2, 0x0a /* Public */,
       7,    0,   98,    2, 0x0a /* Public */,
       8,    2,   99,    2, 0x0a /* Public */,
      10,    0,  104,    2, 0x0a /* Public */,
      11,    0,  105,    2, 0x0a /* Public */,
      12,    0,  106,    2, 0x0a /* Public */,
      13,    0,  107,    2, 0x0a /* Public */,
      14,    2,  108,    2, 0x0a /* Public */,
      15,    0,  113,    2, 0x0a /* Public */,
      16,    0,  114,    2, 0x0a /* Public */,
      17,    0,  115,    2, 0x0a /* Public */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    2,    2,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 9, 0x80000000 | 9,    2,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 9, 0x80000000 | 9,    2,    2,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void QMainCanvas::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<QMainCanvas *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->clicked1(); break;
        case 1: _t->areaFunction(); break;
        case 2: _t->areaFunctionWithBackground(); break;
        case 3: _t->handle_root_events(); break;
        case 4: _t->autoFit((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2]))); break;
        case 5: _t->clearTheScreen(); break;
        case 6: _t->addBackgroundMarker((*reinterpret_cast< Int_t(*)>(_a[1])),(*reinterpret_cast< Int_t(*)>(_a[2]))); break;
        case 7: _t->deleteBackgroundMarkers(); break;
        case 8: _t->deleteAllMarkers(); break;
        case 9: _t->showBackgroundMarkers(); break;
        case 10: _t->showAllMarkers(); break;
        case 11: _t->addRangeMarker((*reinterpret_cast< Int_t(*)>(_a[1])),(*reinterpret_cast< Int_t(*)>(_a[2]))); break;
        case 12: _t->deleteRangeMarkers(); break;
        case 13: _t->showRangeMarkers(); break;
        case 14: _t->FindPeakFunction(); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject QMainCanvas::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_QMainCanvas.data,
    qt_meta_data_QMainCanvas,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *QMainCanvas::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *QMainCanvas::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_QMainCanvas.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int QMainCanvas::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 15;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE

#ifndef XPCHAIN_QT_MINTINGFILTERPROXY_H
#define XPCHAIN_QT_MINTINGFILTERPROXY_H

#include <QSortFilterProxyModel>

class MintingFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit MintingFilterProxy(QObject *parent = 0);
    Qt::ItemFlags flags(const QModelIndex &index) const;
};

#endif // XPCHAIN_QT_MINTINGFILTERPROXY_H

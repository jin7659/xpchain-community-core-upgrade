#include <qt/mintingfilterproxy.h>

MintingFilterProxy::MintingFilterProxy(QObject * parent) :
    QSortFilterProxyModel(parent)
{
}

Qt::ItemFlags MintingFilterProxy::flags(const QModelIndex &index) const
{
    if (!sourceModel()) return Qt::NoItemFlags;
    return QSortFilterProxyModel::flags(index);
}

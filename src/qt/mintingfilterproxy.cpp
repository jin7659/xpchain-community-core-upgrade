#include <qt/mintingfilterproxy.h>

MintingFilterProxy::MintingFilterProxy(QObject * parent) :
    QSortFilterProxyModel(parent)
{
}

Qt::ItemFlags MintingFilterProxy::flags(const QModelIndex &index) const
{
    QAbstractItemModel *src = sourceModel();
    if (!src || !index.isValid())
        return Qt::NoItemFlags;

    const QModelIndex sourceIndex = mapToSource(index);
    if (!sourceIndex.isValid())
        return Qt::NoItemFlags;

    return src->flags(sourceIndex);
}

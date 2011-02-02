#include "xmlParser.h"

#include <QtCore/QDebug>
#include <QtCore/QUuid>
#include <QtXml/QDomDocument>
#include <QtCore/QProcess>
#include <QMessageBox>
#include <QPointF>
#include <QPolygonF>

#include "math.h"

#include "../../../qrrepo/repoApi.h"
#include "../../../utils/xmlUtils.h"
#include "../../editorManager/editorManager.h"
#include "../../kernel/exception/exception.h"

using namespace qReal;
using namespace parsers;

XmlParser::XmlParser(qrRepo::RepoApi &api, EditorManager const &editorManager)
	: mApi(api), mEditorManager(editorManager), mElementsColumn(0), mElementCurrentColumn(0),
	mMoveWidth(180), mMoveHeight(100), mCurrentWidth(0), mCurrentHeight(0), mParentPositionX(280)
{
}

void XmlParser::parseFile(const QString &fileName)
{
	QFileInfo directoryName(fileName);
	QString fileBaseName = directoryName.baseName();

	if (containsName(fileBaseName))
		return;
	QDomDocument const doc = utils::xmlUtils::loadDocument(fileName);

        NewType const packageId = getPackageId();
	initMetamodel(doc, fileName, packageId);

	QDomNodeList const listeners = doc.elementsByTagName("listener");
	int listenerPositionY = 100;
	for (unsigned i = 0; i < listeners.length(); ++i) {
		QDomElement listener = listeners.at(i).toElement();
                NewType type = initListener("(Listener)", listener.attribute("class", ""), listener.attribute("file", ""));
                mApi.setProperty(type, "position", QPointF(0,listenerPositionY));
		listenerPositionY += 90;
	}

	QDomNodeList const diagrams = doc.elementsByTagName("diagram");

	mElementsColumn =  ceil(sqrt(static_cast<qreal>(diagrams.length())));

	for (unsigned i = 0; i < diagrams.length(); ++i) {
		QDomElement diagram = diagrams.at(i).toElement();
		initDiagram(diagram, mMetamodel, diagram.attribute("name", "Unknown Diagram"),
				diagram.attribute("displayedName", "Unknown Diagram"));
	}
	clear();
}

void XmlParser::clear()
{
	mElementsColumn = 0;
	mElementCurrentColumn = 0;
	mMoveWidth = 180;
	mMoveHeight = 100;
	mCurrentWidth = 0;
	mCurrentHeight = 0;
	mParentPositionX = 280;
	mElements.clear();
	mParents.clear();
	mContainers.clear();
}

QStringList XmlParser::getIncludeList(const QString &fileName)
{
	QDomDocument const doc = utils::xmlUtils::loadDocument(fileName);

	QDomNodeList const includeList = doc.elementsByTagName("include");
	QStringList includeFilesList;
	for (unsigned i = 0; i < includeList.length(); ++i) {
		QDomElement include = includeList.at(i).toElement();
		QFileInfo info(fileName);
		QFileInfo name(include.text());
		if (!containsName(name.baseName())) {
			includeFilesList.append(getIncludeList(info.absoluteDir().path() + "/" + include.text()));
			includeFilesList.append(info.absoluteDir().path() + "/" + include.text());
		}
	}
	return includeFilesList;
}

void XmlParser::loadIncludeList(const QString &fileName)
{
	QStringList includeList = getIncludeList(fileName);
	if (includeList.isEmpty())
		return;
	if (QMessageBox::question(NULL, QObject::tr("loading.."),"Do you want to load connected metamodels?", QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes) {
		foreach (QString const &include, includeList) {
			if (!containsName(include))
				parseFile(include);
		}
	}
}

bool XmlParser::containsName(const QString &name)
{
        TypeList idList = mApi.children(ROOT_ID);
        foreach (NewType const &type, idList) {
                if (mApi.name(type) == name)
			return true;
	}
	return false;
}

NewType XmlParser::getPackageId()
{
        TypeList const children = mApi.children(ROOT_ID);
        foreach (NewType type, children) {
                if (type.element() == "PackageDiagram")
                        return type;
	}
        NewType const packageId = NewType("Meta_editor", "MetaEditor", "PackageDiagram");
	setStandartConfigurations(packageId, ROOT_ID, "Package", "Package");
	return packageId;
}

void XmlParser::initMetamodel(const QDomDocument &document, const QString &directoryName, const NewType &type)
{
	QFileInfo fileName(directoryName);
	QString fileBaseName = fileName.baseName();

        NewType metamodelId = NewType("Meta_editor", "MetaEditor", "MetamodelDiagram");

	QDomNodeList const includeList = document.elementsByTagName("include");
	QString includeListString = "";

	for (unsigned i = 0; i < includeList.length(); ++i) {
		QDomElement include = includeList.at(i).toElement();
		includeListString += include.text() + ", ";
	}
        setStandartConfigurations(metamodelId, type, "Empty_" + fileBaseName, "");
	mApi.setProperty(metamodelId, "include", includeListString);
	mApi.setProperty(metamodelId, "name of the directory", fileBaseName);

        mMetamodel = NewType("Meta_editor", "MetaEditor", "MetamodelDiagram");
	setStandartConfigurations(mMetamodel, ROOT_ID, fileBaseName, "");
	mApi.setProperty(mMetamodel, "include", includeListString);
	mApi.setProperty(mMetamodel, "name of the directory", fileBaseName);
	mApi.connect(metamodelId, mMetamodel);
}

NewType XmlParser::initListener(const QString &name, const QString &className, const QString &fileName)
{
        NewType listenerId = NewType("Meta_editor", "MetaEditor", "Listener");
	setStandartConfigurations(listenerId, mMetamodel, name, name);
	mApi.setProperty(listenerId, "class", className);
	mApi.setProperty(listenerId, "file", fileName);
	return listenerId;
}

void XmlParser::initDiagram(const QDomElement &diagram, const NewType &parent,
		const QString &name, const QString &displayedName)
{
        NewType diagramId = NewType("Meta_editor", "MetaEditor", "MetaEditorDiagramNode");

	setStandartConfigurations(diagramId, parent, name, displayedName);
	mApi.setProperty(diagramId, "nodeName", diagram.attribute("nodeName", ""));

	createDiagramAttributes(diagram, diagramId);

	setElementPosition(diagramId);
}

void XmlParser::createDiagramAttributes(const QDomElement &diagram, const NewType &diagramId)
{
	QDomNodeList diagramAttributes = diagram.childNodes();

	for (unsigned i = 0; i < diagramAttributes.length(); ++i) {
		QDomElement type = diagramAttributes.at(i).toElement();
		if (type.tagName() == "nonGraphicTypes")
			createNonGraphicElements(type, diagramId);
		if (type.tagName() == "graphicTypes")
			createGraphicElements(type, diagramId);
	}
	QString const diagramName = mApi.name(diagramId);
	initGeneralization(diagramName);
	initContainer(diagramName);
}

void XmlParser::createNonGraphicElements(const QDomElement &type, const NewType &diagramId)
{
	QDomNodeList enums = type.childNodes();

	for (unsigned i = 0; i < enums.length(); ++i) {
		QDomElement enumElement = enums.at(i).toElement();
		if (enumElement.tagName() == "enum")
			initEnum(enumElement, diagramId);
	}
}

void XmlParser::createGraphicElements(const QDomElement &type, const NewType &diagramId)
{
	QDomNodeList graphicElements = type.childNodes();

	for (unsigned i = 0; i < graphicElements.length(); ++i) {
		QDomElement graphicElement = graphicElements.at(i).toElement();
		if (graphicElement.tagName() == "node")
			initNode(graphicElement, diagramId);
		if (graphicElement.tagName() == "edge")
			initEdge(graphicElement, diagramId);
		if (graphicElement.tagName() == "import")
			initImport(graphicElement, diagramId);
	}
}

void XmlParser::initEnum(const QDomElement &enumElement, const NewType &diagramId)
{
        NewType enumId = NewType("Meta_editor", "MetaEditor", "MetaEntityEnum");

	setStandartConfigurations(enumId, diagramId, enumElement.attribute("name", ""),
			enumElement.attribute("displayedName", ""));

	setEnumAttributes(enumElement, enumId);
}

void XmlParser::initNode(const QDomElement &node, const NewType &diagramId)
{
	QString const nodeName = node.attribute("name", "");

	if (!(mElements.contains(nodeName))) {
                NewType nodeId = NewType("Meta_editor", "MetaEditor", "MetaEntityNode");
		mElements.insert(nodeName, nodeId);

		setStandartConfigurations(nodeId, diagramId, nodeName,
				node.attribute("displayedName", ""));
		mApi.setProperty(nodeId, "path", node.attribute("path", ""));

		setNodeAttributes(node, nodeId);
	}
}

void XmlParser::initEdge(const QDomElement &edge, const NewType &diagramId)
{
	QString const edgeName = edge.attribute("name", "");

	if (!(mElements.contains(edgeName))) {
                NewType edgeId = NewType("Meta_editor", "MetaEditor", "MetaEntityEdge");
		mElements.insert(edgeName, edgeId);

		setStandartConfigurations(edgeId, diagramId, edgeName,
				edge.attribute("displayedName", ""));

		setEdgeAttributes(edge, edgeId);
}
}

void XmlParser::initImport(const QDomElement &import, const NewType &diagramId)
{
	QString const importName = import.attribute("name", "");

	if (!(mElements.contains(importName))) {
                NewType importId = NewType("Meta_editor", "MetaEditor", "MetaEntityImport");
		QStringList nameList = import.attribute("name", "").split("::", QString::SkipEmptyParts);
		setStandartConfigurations(importId, diagramId, nameList.at(1),
				import.attribute("displayedName", ""));
		mApi.setProperty(importId, "as", import.attribute("as", ""));
		mApi.setProperty(importId, "importedFrom", nameList.at(0));
		mElements.insert(importName, importId);
	}
}

void XmlParser::setEnumAttributes(const QDomElement &enumElement, const NewType &enumId)
{
	QDomNodeList values = enumElement.childNodes();

	for (unsigned i = 0; i < values.length(); ++i) {
		QDomElement value = values.at(i).toElement();
		if (value.tagName() == "value"){
                        NewType valueId = NewType("Meta_editor", "MetaEditor", "MetaEntityValue");

			setStandartConfigurations(valueId, enumId, value.text(),
					value.attribute("displayedName", ""));

			mApi.setProperty(valueId, "valueName", value.text());
		}
	}
}

void XmlParser::setNodeAttributes(const QDomElement &node, const NewType &nodeId)
{
	QDomNodeList nodeList = node.childNodes();

	for (unsigned i = 0; i < nodeList.length(); ++i) {
		QDomElement tag = nodeList.at(i).toElement();
		if (tag.tagName() == "logic")
			setNodeConfigurations(tag, nodeId);
		if (tag.tagName() == "graphics") {
			QDomDocument document;
			document.createElement("document");
			QDomNode nodeCopy = nodeList.at(i).cloneNode();
			document.importNode(nodeList.at(i), true);
			document.appendChild(nodeCopy);
			mApi.setProperty(nodeId, "shape", document.toString());
		}
	}
}

void XmlParser::setEdgeAttributes(const QDomElement &edge, const NewType &edgeId)
{
	QDomNodeList edgeList = edge.childNodes();

	for (unsigned i = 0; i < edgeList.length(); ++i) {
		QDomElement tag = edgeList.at(i).toElement();
		if (tag.tagName() == "graphics")
			setLineType(tag, edgeId);
		if (tag.tagName() == "logic")
			setEdgeConfigurations(tag, edgeId);
	}
}

void XmlParser::setNodeConfigurations(const QDomElement &tag, const NewType &nodeId)
{
	QDomNodeList nodeAttributes = tag.childNodes();

	for (unsigned i = 0; i < nodeAttributes.length(); ++i) {
		QDomElement attribute = nodeAttributes.at(i).toElement();
		if (attribute.tagName() == "generalizations")
			setGeneralization(attribute, nodeId);
		else if (attribute.tagName() == "properties")
			setProperties(attribute, nodeId);
		else if (attribute.tagName() == "container")
			setContainers(attribute, nodeId);
		else if (attribute.tagName() == "connections")
			setConnections(attribute, nodeId);
		else if (attribute.tagName() == "usages")
			setUsages(attribute, nodeId);
		else if (attribute.tagName() == "pin")
			setPin(nodeId);
		else if (attribute.tagName() == "action")
			setAction(nodeId);
		else if (attribute.tagName() == "bonusContextMenuFields")
			setFields(attribute, nodeId);
	}
}

void XmlParser::setLineType(const QDomElement &tag, const NewType &edgeId)
{
	QDomNodeList graphics = tag.childNodes();

	if (graphics.length() > 0) {
		QDomElement lineType = graphics.at(0).toElement();
		mApi.setProperty(edgeId, "lineType", lineType.attribute("type", ""));
	}
	// quick workaround for #349, just saving a part of XML into `labels' property
	// TODO: make it somehow more elegant
	for(unsigned i = 0; i < graphics.length(); ++i){
		QDomElement element = graphics.at(i).toElement();
		if (element.tagName() == "labels"){
			QString labels;
			QTextStream out(&labels);
			element.save(out, 4);
			mApi.setProperty(edgeId, "labels", labels);
		}
	}
}

void XmlParser::setEdgeConfigurations(const QDomElement &tag, const NewType &edgeId)
{
	QDomNodeList edgeAttributes = tag.childNodes();

	for (unsigned i = 0; i < edgeAttributes.length(); ++i) {
		QDomElement attribute = edgeAttributes.at(i).toElement();
		if (attribute.tagName() == "generalizations")
			setGeneralization(attribute, edgeId);
		else if (attribute.tagName() == "properties")
			setProperties(attribute, edgeId);
		else if (attribute.tagName() == "associations")
			setAssociations(attribute, edgeId);
		else if (attribute.tagName() == "possibleEdges")
			setPossibleEdges(attribute, edgeId);
	}
}

void XmlParser::setGeneralization(const QDomElement &element, const NewType &elementId)
{
	QDomNodeList generalizations = element.childNodes();
	QStringList parents;

	for (unsigned i = 0; i < generalizations.length(); ++i) {
		QDomElement generalization = generalizations.at(i).toElement();
		if (generalization.tagName() == "parent")
				parents.insert(0, generalization.attribute("parentName", ""));
		mParents.insert(elementId, parents);
	}
}

void XmlParser::setProperties(const QDomElement &element, const NewType &elementId)
{
	QDomNodeList properties = element.childNodes();

	for (unsigned i = 0; i < properties.length(); ++i) {
		QDomElement property = properties.at(i).toElement();
		if (property.tagName() == "property")
			initProperty(property, elementId);
	}
}

void XmlParser::setFields(const QDomElement &element, const NewType &elementId)
{
	QDomNodeList fields = element.childNodes();

	for (unsigned i = 0; i < fields.length(); ++i) {
		QDomElement field = fields.at(i).toElement();
		if (field.tagName() == "field") {
                        NewType fieldId = NewType("Meta_editor", "MetaEditor", "MetaEntityContextMenuField");
			setStandartConfigurations(fieldId, elementId, field.attribute("name", ""),
					field.attribute("displayedName", ""));
		}
	}
}

void XmlParser::setContainers(const QDomElement &element, const NewType &elementId)
{
	QDomNodeList containsElements = element.childNodes();
	QStringList containers;
	for (unsigned i = 0; i < containsElements.length(); ++i) {
		QDomElement contains = containsElements.at(i).toElement();
		if (contains.tagName() == "contains") {
			QString type = contains.attribute("type", "");
			containers.insert(0, type);
			/*QString existingContainers;
			if (mApi.hasProperty(elementId, "container"))
				existingContainers = mApi.stringProperty(elementId, "container");
			if (!existingContainers.isEmpty())
				existingContainers += ",";
			existingContainers += type;

			mApi.setProperty(elementId, "container", existingContainers);*/
		}
		if (contains.tagName() == "properties")
			setContainerProperties(contains, elementId);
	}
	mContainers.insert(elementId, containers);
}

void XmlParser::setContainerProperties(const QDomElement &element, const NewType &elementId)
{
	QDomNodeList properties = element.childNodes();
	if (properties.size() > 0) {
                NewType containerProperties = NewType("Meta_editor", "MetaEditor",
                                "MetaEntityPropertiesAsContainer");
		setStandartConfigurations(containerProperties, elementId,
				"properties", "");
		for (unsigned i = 0; i < properties.length(); ++i) {
			QDomElement property = properties.at(i).toElement();
			setBoolValuesForContainer("sortContainer", property, containerProperties);
			setBoolValuesForContainer("minimizeToChildren", property, containerProperties);
			setBoolValuesForContainer("maximizeChildren", property, containerProperties);
			setBoolValuesForContainer("banChildrenMove", property, containerProperties);

			setSizesForContainer("forestalling", property, containerProperties);
			setSizesForContainer("childrenForestalling", property, containerProperties);
		}
	}
}

void XmlParser::setBoolValuesForContainer(const QString &tagName, const QDomElement &property, const NewType &type)
{
	if (property.tagName() == tagName)
                mApi.setProperty(type, tagName, "true");
}

void XmlParser::setSizesForContainer(const QString &tagName, const QDomElement &property, const NewType &type)
{
	if (property.tagName() == tagName)
                mApi.setProperty(type, tagName + "Size", property.attribute("size", "0"));
}

void XmlParser::setConnections(const QDomElement &element, const NewType &elementId)
{
	QDomNodeList connections = element.childNodes();

	for (unsigned i = 0; i < connections.length(); ++i) {
		QDomElement connection = connections.at(i).toElement();
		if (connection.tagName() == "connection")
			initConnection(connection, elementId);
	}
}

void XmlParser::setUsages(const QDomElement &element, const NewType &elementId)
{
	QDomNodeList usages = element.childNodes();

	for (unsigned i = 0; i < usages.length(); ++i) {
		QDomElement usage = usages.at(i).toElement();
		if (usage.tagName() == "usage")
			initUsage(usage, elementId);
	}
}

void XmlParser::setAssociations(const QDomElement &element, const NewType &elementId)
{
        NewType associationId = NewType("Meta_editor", "MetaEditor", "MetaEntityAssociation");
	QDomNodeList associations = element.childNodes();

	QDomElement association = associations.at(0).toElement();

	setStandartConfigurations(associationId, elementId, association.attribute("name", ""),
			association.attribute("displayedName", ""));

	mApi.setProperty(associationId, "beginType", element.attribute("beginType", ""));
	mApi.setProperty(associationId, "endType", element.attribute("endType", ""));
	mApi.setProperty(associationId, "beginName", association.attribute("beginName", ""));
	mApi.setProperty(associationId, "endName", association.attribute("endName", ""));
}

void XmlParser::setPossibleEdges(const QDomElement &element, const NewType &elementId)
{
	QDomNodeList possibleEdges = element.childNodes();

	for (unsigned i = 0; i < possibleEdges.length(); ++i) {
		QDomElement possibleEdge = possibleEdges.at(i).toElement();
		if (possibleEdge.tagName() == "possibleEdge")
			initPossibleEdge(possibleEdge, elementId);
	}
}

void XmlParser::setPin(const NewType &elementId)
{
	mApi.setProperty(elementId, "isPin", "true");
}

void XmlParser::setAction(const NewType &elementId)
{
	mApi.setProperty(elementId, "isAction", "true");
}

void XmlParser::initPossibleEdge(const QDomElement &possibleEdge, const NewType &elementId)
{
        NewType possibleEdgeId = NewType("Meta_editor", "MetaEditor", "MetaEntityPossibleEdge");

	setStandartConfigurations(possibleEdgeId, elementId, possibleEdge.attribute("name", ""),
			possibleEdge.attribute("displayedName", ""));

	mApi.setProperty(possibleEdgeId, "beginName", possibleEdge.attribute("beginName", ""));
	mApi.setProperty(possibleEdgeId, "endName", possibleEdge.attribute("endName", ""));
	mApi.setProperty(possibleEdgeId, "directed", possibleEdge.attribute("directed", "false"));
}

void XmlParser::initProperty(const QDomElement &property, const NewType &elementId)
{
        NewType propertyId = NewType("Meta_editor", "MetaEditor", "MetaEntity_Attribute");

	setStandartConfigurations(propertyId, elementId, property.attribute("name", ""),
			property.attribute("displayedName", ""));

	mApi.setProperty(propertyId, "type", property.attribute("type", ""));
	mApi.setProperty(propertyId, "attributeType", property.attribute("type", "0"));

	QDomNodeList defaultValue = property.childNodes();
	if (!defaultValue.isEmpty())
		mApi.setProperty(propertyId, "defaultValue",
				defaultValue.at(0).toElement().text());
}

void XmlParser::initConnection(const QDomElement &connection, const NewType &elementId)
{
        NewType connectionId = NewType("Meta_editor", "MetaEditor", "MetaEntityConnection");

	setStandartConfigurations(connectionId, elementId, connection.attribute("name", ""),
			connection.attribute("displayedName", ""));

	mApi.setProperty(connectionId, "type", connection.attribute("type", ""));
}

void XmlParser::initUsage(const QDomElement &usage, const NewType &elementId)
{
        NewType usageId = NewType("Meta_editor", "MetaEditor", "MetaEntityUsage");

	setStandartConfigurations(usageId, elementId, usage.attribute("name", ""),
			usage.attribute("displayedName", ""));

	mApi.setProperty(usageId, "type", usage.attribute("type", ""));
}

void XmlParser::initGeneralization(const QString &diagramName)
{
        foreach (NewType const type, mParents.keys()) {
                setParents(type, diagramName);
	}
}

void XmlParser::initContainer(const QString &diagramName)
{
        foreach (NewType const type, mContainers.keys()) {
                setContains(type, diagramName);
	}
}

void XmlParser::setParents(const NewType &type, const QString &diagramName)
{
        TypeList parents;
        foreach (QString const elementName, mParents[type]) {
		QStringList name = elementName.split("::");
		QString baseElementName;
		if (name.size() < 2)
			baseElementName = elementName;
		else
			baseElementName = (name[0] == diagramName) ? name[1] : elementName;
		if ((mElements.contains(baseElementName))) {
                        initInheritance(mElements[baseElementName], type);
		}
		else {
                        NewType const parentId = getParentId(baseElementName);
			parents.append(parentId);
                        initInheritance(parentId, type);
			mElements.insert(baseElementName, parentId);
		}
	}
	if (!parents.isEmpty())
		manageParents(parents);
}

void XmlParser::initInheritance(const NewType &idFrom, const NewType &idTo)
{
        NewType inheritanceId = NewType("Meta_editor", "MetaEditor", "Inheritance");

	QString const name = mApi.name(idTo) + "_Inherits_" + mApi.name(idFrom);

	setStandartConfigurations(inheritanceId, mMetamodel, name, name);
	mApi.setProperty(inheritanceId, "from", idFrom.toVariant());
	mApi.setProperty(inheritanceId, "to", idTo.toVariant());
}

NewType XmlParser::getParentId(const QString &elementName)
{
        NewType parentId = NewType("Meta_editor", "MetaEditor", "MetaEntityImport");

	setStandartConfigurations(parentId, mMetamodel, elementName, elementName);
	return parentId;
}

void XmlParser::setContains(const NewType &type, const QString &diagramName)
{
        foreach (QString const elementName, mContainers[type]) {
		QStringList name = elementName.split("::");
		QString baseElementName;
		if (name.size() < 2)
			baseElementName = elementName;
		else
		baseElementName = (name[0] == diagramName) ? name[1] : elementName;
		if ((mElements.contains(baseElementName))) {
                        initContains(type, mElements[baseElementName]);
		}
	}
}

void XmlParser::initContains(const NewType &idFrom, const NewType &idTo)
{
        NewType containerId = NewType("Meta_editor", "MetaEditor", "Container");
	QString const name = mApi.name(idFrom) + "_Contains_" + mApi.name(idTo);

	setStandartConfigurations(containerId, mMetamodel, name, name);
	mApi.setProperty(containerId, "from", idFrom.toVariant());
	mApi.setProperty(containerId, "to", idTo.toVariant());
}

void XmlParser::manageParents(const TypeList &parents)
{
        foreach (NewType const type, parents) {
                mApi.setProperty(type, "position", QPointF(mParentPositionX, 0));
		mParentPositionX += 120;
	}
}

void XmlParser::setStandartConfigurations(NewType const &type, NewType const &parent,
		const QString &name, const QString &displayedName)
{
        if (!mEditorManager.hasElement(type))
                throw Exception(QString("%1 doesn't exist").arg(type.toString()));
        mApi.addChild(parent, type);
        mApi.setProperty(type, "name", name);
	if (displayedName != "")
                mApi.setProperty(type, "displayedName", displayedName);
        mApi.setProperty(type, "from", ROOT_ID.toVariant());
        mApi.setProperty(type, "to", ROOT_ID.toVariant());
        mApi.setProperty(type, "fromPort", 0.0);
        mApi.setProperty(type, "toPort", 0.0);
        mApi.setProperty(type, "links", TypeListHelper::toVariant(TypeList()));
        mApi.setProperty(type, "outgoingConnections", TypeListHelper::toVariant(TypeList()));
        mApi.setProperty(type, "incomingConnections", TypeListHelper::toVariant(TypeList()));
        mApi.setProperty(type, "outgoingUsages", TypeListHelper::toVariant(TypeList()));
        mApi.setProperty(type, "incomingUsages", TypeListHelper::toVariant(TypeList()));

        mApi.setProperty(type, "position", QPointF(0,0));
        mApi.setProperty(type, "configuration", QVariant(QPolygon()));
}

void XmlParser::setChildrenPositions(const NewType &type, unsigned cellWidth, unsigned cellHeight)
{
        int rowWidth = ceil(sqrt(static_cast<qreal>(mApi.children(type).count())));
	int currentRow = 0;
	int currentColumn = 0;
	int sizeyElement = 100;
	int sizeyElements = 0;

        foreach(NewType element, mApi.children(type)) {
		mApi.setProperty(element, "position", QPointF(currentColumn * (cellWidth + 40) + 50, sizeyElement));
		if (mApi.children(element).isEmpty())
			sizeyElement += 180;
		else
			sizeyElement += cellHeight * mApi.children(element).length() + 80;
		++currentRow;
		if (currentRow >= rowWidth) {
			currentRow = 0;
			++currentColumn;
			if (sizeyElement > sizeyElements)
				sizeyElements = sizeyElement;
			sizeyElement = 100;
		}
	}
	mCurrentWidth = rowWidth * cellWidth + 30;
	mCurrentHeight = sizeyElements;
}

void XmlParser::checkIndex()
{
	++mElementCurrentColumn;
	if (mElementCurrentColumn >= mElementsColumn) {
		mElementCurrentColumn = 0;
		mMoveHeight = 0;
		mMoveWidth += mCurrentWidth;
	}
}

void XmlParser::setElementPosition(const NewType &type)
{
	mMoveHeight += mCurrentHeight;
        mApi.setProperty(type, "position", QPointF(mMoveWidth, mMoveHeight));
        setChildrenPositions(type, 160, 50);
	QRect const value = QRect(mMoveWidth, mMoveHeight, mCurrentWidth, mCurrentHeight);
        mApi.setProperty(type, "configuration", QVariant(QPolygon(value, false)));
	checkIndex();
}

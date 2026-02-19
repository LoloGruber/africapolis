${
function groupShapefilesByNameroot(files) {
    var grouped = {};
    files.forEach(function(f) {
      var nameroot = f.nameroot || f.basename.replace(/\.[^/.]+$/, "");
      if (!grouped[nameroot]) {
        grouped[nameroot] = {};
      }
      if (f.basename.endsWith(".shp")) grouped[nameroot].shp = f;
      if (f.basename.endsWith(".shx")) grouped[nameroot].shx = f;
      if (f.basename.endsWith(".dbf")) grouped[nameroot].dbf = f;
      if (f.basename.endsWith(".prj")) grouped[nameroot].prj = f;
      if (f.basename.endsWith(".cpg")) grouped[nameroot].cpg = f;
      if (f.basename.endsWith(".qpj")) grouped[nameroot].qpj = f;
    });
    return Object.values(grouped).filter(f => f && f.shp);;
}

function groupToShapefileObject(group) {
    if (!group.shp) return null;
    return {
        "file":{
            class: "File",
            path: group.shp.path,
            basename: group.shp.basename,
            nameroot: group.shp.nameroot,
            nameext: group.shp.nameext,
            secondaryFiles: [group.shx, group.dbf, group.prj,group.cpg,group.qpj].filter(Boolean)
        }
    };
}

var shapefiles = groupShapefilesByNameroot(self);
if(shapefiles.length == 1)
    return groupToShapefileObject(shapefiles.at(0));
return shapefiles.map(groupToShapefileObject).filter(Boolean);
}
  
/*
 * Copyright (C) 2014 Project Hatohol
 *
 * This file is part of Hatohol.
 *
 * Hatohol is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Hatohol is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Hatohol. If not, see <http://www.gnu.org/licenses/>.
 */

var HatoholUserRolesEditor = function(params) {
  var self = this;
  var dialogButtons = [{
    text: gettext("CLOSE"),
    click: closeButtonClickedCb
  }];
  self.operatorProfile = params.operatorProfile;
  self.changedCallback = params.changedCallback;
  self.mainTableId = "userRoleEditorMainTable";
  self.userRolesData = null;
  self.changed = false;

  // call the constructor of the super class
  var dialogAttrs = { width: "600" };
  HatoholDialog.apply(
    this, ["user-roles-editor", gettext("EDIT USER ROLES"),
           dialogButtons, dialogAttrs]);

  //
  // Dialog button handlers
  //
  function closeButtonClickedCb() {
    self.closeDialog();
    if (self.changed && self.changedCallback)
      self.changedCallback();
  }

  function deleteUserRoles() {
    var deleteList = [], id;
    var checkboxes = $(".userRoleSelectCheckbox");
    $(this).dialog("close");
    for (var i = 0; i < checkboxes.length; i++) {
      if (!checkboxes[i].checked)
        continue;
      id = checkboxes[i].getAttribute("userRoleId");
      deleteList.push(id);
    }
    new HatoholItemRemover({
      id: deleteList,
      type: "user-role",
      completionCallback: function() {
        self.load();
        self.changed = true;
      }
    });
    hatoholInfoMsgBox(gettext("Deleting..."));
  }

  $("#addUserRoleButton").click(function() {
    new HatoholUserRoleEditor({
      operatorProfile: self.operatorProfile,
      succeededCallback: function() {
        self.load();
        self.changed = true;
      },
      userRoles: self.userRoles
    });
  });

  $("#deleteUserRolesButton").click(function() {
    var msg = gettext("Are you sure to delete selected items?");
    hatoholNoYesMsgBox(msg, deleteUserRoles);
  });

  self.load();
};

HatoholUserRolesEditor.prototype = Object.create(HatoholDialog.prototype);
HatoholUserRolesEditor.prototype.constructor = HatoholUserRolesEditor;

HatoholUserRolesEditor.prototype.load = function() {
  var self = this;

  new HatoholConnector({
    url: "/user-role",
    request: "GET",
    data: {},
    replyCallback: function(userRolesData, parser) {
      self.userRolesData = userRolesData;
      self.updateMainTable();
    },
    parseErrorCallback: hatoholErrorMsgBoxForParser,
    connectErrorCallback: function(XMLHttpRequest, textStatus, errorThrown) {
      var errorMsg = "Error: " + XMLHttpRequest.status + ": " +
                     XMLHttpRequest.statusText;
      hatoholErrorMsgBox(errorMsg);
    }
  });
};

HatoholUserRolesEditor.prototype.updateMainTable = function() {
  var self = this;
  var numSelected = 0;
  var setupCheckboxes = function() {
    $(".userRoleSelectCheckbox").change(function() {
      var check = $(this).is(":checked");
      var prevNumSelected = numSelected;
      if (check)
        numSelected += 1;
      else
        numSelected -= 1;
      if (prevNumSelected == 0 && numSelected == 1)
        $("#deleteUserRolesButton").attr("disabled", false);
      else if (prevNumSelected == 1 && numSelected == 0)
        $("#deleteUserRolesButton").attr("disabled", true);
    });
  };
  var setupEditButtons = function()
  {
    var userRoles = self.userRolesData.userRoles, userRolesMap = {};
    var i, id;

    for (i = 0; i < userRoles.length; ++i)
      userRolesMap[userRoles[i]["userRoleId"]] = userRoles[i];

    for (i = 0; i < userRoles.length; ++i) {
      id = "#editUserRole" + userRoles[i]["userRoleId"];
      $(id).click(function() {
        var userRoleId = this.getAttribute("userRoleId");
        new HatoholUserRoleEditor({
          operatorProfile: self.operatorProfile,
          succeededCallback: function() {
            self.load();
            self.changed = true;
          },
          targetUserRole: userRolesMap[userRoleId],
          userRoles: userRoles
        });
      });
    }
  };

  if (!this.userRolesData)
    return;

  var rows = this.generateTableRows(this.userRolesData);
  var tbody = $("#" + this.mainTableId + " tbody");
  tbody.empty().append(rows);
  setupCheckboxes();
  setupEditButtons();
  this.setupWidgetsState();
};

HatoholUserRolesEditor.prototype.generateMainTable = function() {
  var html =
  '<form class="form-inline">' +
  '  <input id="addUserRoleButton" type="button" ' +
  '    class="addUserRole form-control" value="' + gettext("ADD") + '" />' +
  '  <input id="deleteUserRolesButton" type="button" disabled ' +
  '    class="deleteUserRole form-control" value="' + gettext("DELETE") + '" />' +
  '</form>' +
  '<div class="ui-widget-content" style="overflow-y: auto; height: 200px">' +
  '<table class="table table-condensed table-striped table-hover" id=' +
  this.mainTableId + '>' +
  '  <thead>' +
  '    <tr>' +
  '      <th class="deleteUserRole"> </th>' +
  '      <th>ID</th>' +
  '      <th>' + gettext("Name") + '</th>' +
  '      <th>' + gettext("Privilege") + '</th>' +
  '    </tr>' +
  '  </thead>' +
  '  <tbody></tbody>' +
  '</table>' +
  '</div>';
  return html;
};

HatoholUserRolesEditor.prototype.generateTableRows = function(data) {
  var html = '', role;

  html += '<tr><td class="deleteUserRole"></td><td>-</td><td>' +
    gettext('Guest') + '</td><td>' + gettext("None") +'</td></tr>';
  html += '<tr><td class="deleteUserRole"></td><td>-</td><td>' +
    gettext('Admin') + '</td><td>' + gettext("All") +'</td></tr>';

  for (var i = 0; i < data.userRoles.length; i++) {
    role = data.userRoles[i];
    html +=
    '<tr>' +
    '<td class="deleteUserRole">' +
    '  <input type="checkbox" class="userRoleSelectCheckbox" ' +
    '         userRoleId="' + escapeHTML(role.userRoleId) + '"></td>' +
    '<td>' + escapeHTML(role.userRoleId) + '</td>' +
    '<td>' + escapeHTML(role.name) + '</td>' +
    '<td>' +
    '<form class="form-inline" style="margin: 0">' +
    '  <input id="editUserRole' + escapeHTML(role["userRoleId"]) + '"' +
    '    type="button" class="btn btn-default editUserRole"' +
    '    userRoleId="' + escapeHTML(role["userRoleId"]) + '"' +
    '    value="' + gettext("Show / Edit") + '" />' +
    '</form>' +
    '</td>' +
    '</tr>';
  }
  return html;
};

HatoholUserRolesEditor.prototype.createMainElement = function() {
  var self = this;
  var element = $(this.generateMainTable());
  return element;
};

HatoholUserRolesEditor.prototype.hasPrivilege = function (privilege) {
  if (!this.operatorProfile)
    return false;
  return this.operatorProfile.hasFlag(privilege);
};

HatoholUserRolesEditor.prototype.setupWidgetsState = function () {
  if (this.hasPrivilege(hatohol.OPPRVLG_CREATE_USER_ROLE))
    $(".addUserRole").show();
  else
    $(".addUserRole").hide();

  if (this.hasPrivilege(hatohol.OPPRVLG_DELETE_ALL_USER_ROLE))
    $(".deleteUserRole").show();
  else
    $(".deleteUserRole").hide();

  if (this.hasPrivilege(hatohol.OPPRVLG_EDIT_ALL_USER_ROLE))
    $(".editUserRole").val(gettext("Show / Edit"));
  else
    $(".editUserRole").val(gettext("Show"));
};

HatoholUserRolesEditor.prototype.onAppendMainElement = function () {
  this.setupWidgetsState();
};


var HatoholUserRoleEditor = function(params) {
  var self = this;

  self.operatorProfile = params.operatorProfile;
  self.userRole = params.targetUserRole;
  self.userRoles = params.userRoles;
  self.succeededCallback = params.succeededCallback;
  self.windowTitle = self.userRole ?
    gettext("EDIT USER ROLE") : gettext("ADD USER ROLE");
  self.applyButtonTitle = self.userRole ? gettext("APPLY") : gettext("ADD");

  var dialogButtons = [{
    text: self.applyButtonTitle,
    click: applyButtonClickedCb
  }, {
    text: gettext("CANCEL"),
    click: cancelButtonClickedCb
  }];

  // call the constructor of the super class
  dialogAttrs = { width: "auto" };
  HatoholDialog.apply(
    this, ["user-role-editor", self.windowTitle,
           dialogButtons, dialogAttrs]);

  //
  // Dialog button handlers
  //
  function applyButtonClickedCb() {
    if (validateParameters()) {
      makeQueryData();
      if (self.userRole)
        hatoholInfoMsgBox(gettext("Now updating the user role ..."));
      else
        hatoholInfoMsgBox(gettext("Now creating a user role ..."));
      postAddUserRole();
    }
  }

  function cancelButtonClickedCb() {
    self.closeDialog();
  }

  function getPrivilegeFlags() {
    var i, flags = 0;
    var privileges = self.hatoholPrivileges;
    for (i = 0; i < privileges.length; ++i) {
      var checked = $("#privilegeFlagId" + i).is(":checked");
      if (checked)
        flags |= (1 << privileges[i].flag);
    }
    return flags;
  }

  function makeQueryData() {
      var queryData = {};
      queryData.name = $("#editUserRoleName").val();
      queryData.flags = getPrivilegeFlags();
      return queryData;
  }

  function postAddUserRole() {
    var url = "/user-role";
    if (self.userRole)
      url += "/" + self.userRole.userRoleId;
    new HatoholConnector({
      url: url,
      request: self.userRole ? "PUT" : "POST",
      data: makeQueryData(),
      replyCallback: replyCallback,
      parseErrorCallback: hatoholErrorMsgBoxForParser
    });
  }

  function replyCallback(reply, parser) {
    self.closeDialog();
    if (self.userRole)
      hatoholInfoMsgBox(gettext("Successfully updated."));
    else
      hatoholInfoMsgBox(gettext("Successfully created."));

    if (self.succeededCallback)
      self.succeededCallback();
  }

  function validateParameters() {
    var i, role, msg;
    var name = $("#editUserRoleName").val();
    var flags = getPrivilegeFlags();
    var userRoles = [
      {
        name: gettext("Guest"),
        flags: hatohol.NONE_PRIVILEGE
      },
      {
        name: gettext("Admin"),
        flags: hatohol.ALL_PRIVILEGES
      }
    ];
    if (self.userRoles)
      userRoles = userRoles.concat(self.userRoles);

    if ($("#editUserRoleName").val() == "") {
      hatoholErrorMsgBox(gettext("User role name is empty!"));
      return false;
    }

    for (i = 0; userRoles && i < userRoles.length; ++i) {
      role = userRoles[i];
      if (self.userRole && self.userRole.userRoleId == role.userRoleId)
        continue;
      if (name == role.name) {
        hatoholErrorMsgBox(gettext("The user role name is already used!"));
        return false;
      }
      if (flags == role.flags) {
        msg = gettext("Same privilege is already defined as: ");
        msg += role.name;
        hatoholErrorMsgBox(msg);
        return false;
      }
    }
    return true;
  }
};

HatoholUserRoleEditor.prototype = Object.create(HatoholDialog.prototype);
HatoholUserRoleEditor.prototype.constructor = HatoholUserRoleEditor;

HatoholUserRoleEditor.prototype.hatoholPrivileges = [
  {
    flag: hatohol.OPPRVLG_CREATE_USER,
    message: gettext("Create a user"),
    category: gettext("Users")
  },
  {
    flag: hatohol.OPPRVLG_UPDATE_USER,
    message: gettext("Update users"),
    category: gettext("Users")
  },
  {
    flag: hatohol.OPPRVLG_DELETE_USER,
    message: gettext("Delete users"),
    category: gettext("Users")
  },
  {
    flag: hatohol.OPPRVLG_GET_ALL_USER,
    message: gettext("Get all users"),
    category: gettext("Users")
  },
  {
    flag: hatohol.OPPRVLG_CREATE_SERVER,
    message: gettext("Create a server"),
    category: gettext("Servers")
  },
  {
    flag: hatohol.OPPRVLG_UPDATE_SERVER,
    message: gettext("Update allowed servers"),
    category: gettext("Servers")
  },
  {
    flag: hatohol.OPPRVLG_UPDATE_ALL_SERVER,
    message: gettext("Update all servers"),
    category: gettext("Servers")
  },
  {
    flag: hatohol.OPPRVLG_DELETE_SERVER,
    message: gettext("Delete allowed servers"),
    category: gettext("Servers")
  },
  {
    flag: hatohol.OPPRVLG_DELETE_ALL_SERVER,
    message: gettext("Delete all servers"),
    category: gettext("Servers")
  },
  {
    flag: hatohol.OPPRVLG_GET_ALL_SERVER,
    message: gettext("Get all servers"),
    category: gettext("Servers")
  },
  {
    flag: hatohol.OPPRVLG_CREATE_ACTION,
    message: gettext("Create a action"),
    category: gettext("Actions")
  },
  {
    flag: hatohol.OPPRVLG_UPDATE_ACTION,
    message: gettext("Update own actions"),
    category: gettext("Actions")
  },
  {
    flag: hatohol.OPPRVLG_UPDATE_ALL_ACTION,
    message: gettext("Update all actions"),
    category: gettext("Actions")
  },
  {
    flag: hatohol.OPPRVLG_DELETE_ACTION,
    message: gettext("Delete own actions"),
    category: gettext("Actions")
  },
  {
    flag: hatohol.OPPRVLG_DELETE_ALL_ACTION,
    message: gettext("Delete all actions"),
    category: gettext("Actions")
  },
  {
    flag: hatohol.OPPRVLG_GET_ALL_ACTION,
    message: gettext("Get all actions"),
    category: gettext("Actions")
  },
  {
    flag: hatohol.OPPRVLG_CREATE_USER_ROLE,
    message: gettext("Create a user role"),
    category: gettext("User roles")
  },
  {
    flag: hatohol.OPPRVLG_UPDATE_ALL_USER_ROLE,
    message: gettext("Update all user roles"),
    category: gettext("User roles")
  },
  {
    flag: hatohol.OPPRVLG_DELETE_ALL_USER_ROLE,
    message: gettext("Delete all user roles"),
    category: gettext("User roles")
  },
];

HatoholUserRoleEditor.prototype.createMainElement = function() {
  var name = this.userRole ? this.userRole.name : "";
  var flags = this.userRole ? this.userRole.flags : 0;
  var i, privileges = this.hatoholPrivileges;
  var html = '<div>';

  // User role name
  html +=
  '<label for="editUserRoleName">' + gettext("User role name") + '</label>' +
  '<br>' +
  '<input id="editUserRoleName" type="text" value="' + escapeHTML(name) + '"' +
  '       class="input-xlarge editUserRoleProp">' +
  '<br>';

  // Checkboxes for privilege flags
  html +=
  '<label>' + gettext("Privileges") + '</label>' +
  '<div class="ui-widget-content" style="overflow-y: scroll; height: 200px">' +
  '<table class="table table-condensed table-striped table-hover">' +
  '<tbody>';
  for (i = 0; i < privileges.length; ++i) {
    var checked = (flags & (1 << privileges[i].flag)) ? "checked" : "";
    html +=
    '<tr>' +
    '<td><input type="checkbox" class="privilegeCheckbox editUserRoleProp" '+
    checked + '  id="privilegeFlagId' + i + '"></td>' +
    '<td>' + privileges[i].message + '</td>' +
    '</tr>';
  }
  html += '</tbody></table></div>';

  html += '</div>';
  return $(html);
};

HatoholUserRoleEditor.prototype.hasPrivilege = function (privilege) {
  if (!this.operatorProfile)
    return false;
  return this.operatorProfile.hasFlag(privilege);
};

HatoholUserRoleEditor.prototype.onAppendMainElement = function () {
  var widgets = $(".editUserRoleProp");
  if (!this.hasPrivilege(hatohol.OPPRVLG_EDIT_ALL_USER_ROLE))
    widgets.attr("disabled", "disabled");
};

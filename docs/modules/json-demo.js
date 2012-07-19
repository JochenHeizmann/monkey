//
// json-docs: generate dynamic docs from json data
// by Matt Sephton <http://www.gingerbeardman.com>
//
// note: it might be easier if you auto generate your data!
// using something like george.monkey
//
// to add your own docs to the system, we do the following:
// 1. push new content onto the searchData array
// 2. define new content data to be included in the navigation
// 3. define new modules to be included in the navigation
// 4. include this .js file in the main doc .html file, using:
// <script type="text/javascript" charset="utf-8" src="json-demo.js"></script>
// ...that's it!

// search data consists of an array of id/text pairs
// id: is the link that is followed for search results
// text: is the text that will appear in the search list
//
searchData.push({
	"id": "demo",
	"text": "demo"
}, {
	"id": "demo.module",
	"text": "demo.module"
}, {
	"id": "demo.module/FunctionName",
	"text": "FunctionName"
}, {
	"id": "demo.module/Module",
	"text": "module"
}, {
	"id": "demo.module/Module.ClassName",
	"text": "Module.ClassName"
});

// content data is an array of content entries
// for each content entry there must be at least one .main and one .contents entry:
// .main contains the breadcrumb trail and the definition of the module
//   breadcrumbs: is an array of name/link pairs
//   definition: consists of arbritrary fields, usually a heading/introductory pair
// .contents is an array of sub nav items, each containing name/link pairs
// it can contain as many sections as you wish in this example demo.module has three:
// globals, functions and classes but they can be named whatever you wish.
// 
// each sub nav item also needs a content entry, consisting of breadcrumbs and definition
// here again definition can contain arbritrarily named fields but there are special names:
// heading: becomes the page heading
// syntax: presented as a prominent highlight
// parameters: presented as a second level highlight
// example: presented in a way that allows <pre> formatted source code
//
var demoData = {
	"demo.main": {
		"breadcrumbs": [{
			"name": "demo",
			"link": ""
		}],
		"definition": [{
			"heading": "Module <strong>demo</strong>",
			"introduction": ""
		}]
	},
	"demo.contents": {},
	"demo.module.main": {
		"breadcrumbs": [{
			"name": "demo.module",
			"link": ""
		}],
		"definition": [{
			"heading": "Module <strong>demo.module</strong>",
			"introduction": "<p>Lorem ipsum dolor sit amet.</p>"
		}]
	},
	"demo.module.contents": {
		"globals": [{
			"name": "Test",
			"link": "Test"
		}],
		"functions": [{
			"name": "FunctionName",
			"link": "FunctionName"
		}],
		"classes": [{
			"name": "Module",
			"link": "Module"
		}, {
			"name": "Module.ClassName",
			"link": "Module.ClassName"
		}]
	},
	"demo.module.Module": {
		"breadcrumbs": [{
			"name": "demo.module",
			"link": "#"
		}, {
			"name": "Module",
			"link": "Module"
		}],
		"definition": [{
			"heading": "Class <strong>Module</strong>",
			"introduction": "<p>Lorem ipsum dolor sit amet.</p>",
			"example": "<p></p>"
		}]
	},
	"demo.module.Module.ClassName": {
		"breadcrumbs": [{
			"name": "demo.module",
			"link": "#"
		}, {
			"name": "App.ClassName",
			"link": "App.ClassName"
		}],
		"definition": [{
			"heading": "Method <strong>ClassName</strong>",
			"introduction": "",
			"syntax": "<p>Method <b>ClassName</b>()</p>",
			"description": "<p>Lorem ipsum dolor sit amet.</p>",
			"example": "<p><pre>source code</pre></p>"
		}]
	},
	"demo.module.FunctionName": {
		"breadcrumbs": [{
			"name": "demo.module",
			"link": "#"
		}, {
			"name": "FunctionName",
			"link": "FunctionName"
		}],
		"definition": [{
			"heading": "Function <strong>FunctionName</strong>",
			"introduction": "",
			"syntax": "<p>Function <b>FunctionName</b>:String()</p>",
			"description": "<p>Lorem ipsum dolor sit amet.</p>",
			"see also": "<p><b>ClassName</b></p>",
			"example": "<p><pre>source code</pre></p>"
		}]
	}
};

// module data is an array of top level module names as name/link pairs
// both are currently the same, but may become different in future
// name: display name
// link: url slug
//
var demoModules = [{
	"name": "demo",
	"link": "demo"
}, {
	"name": "demo.module",
	"link": "demo.module"
}];

//
// note: do not modify the code below.
//
// this code extends the existing content with the new content
files = $.extend({}, files, demoData);
//
//this code merges the new modules into the existing nav data
oldModules = $.parseJSON(files["modules.list"]);
newModules = $.merge(oldModules.modules, demoModules);
files["modules.list"] = '{"modules":'+JSON.stringify(newModules)+'}';

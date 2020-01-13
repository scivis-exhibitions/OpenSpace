import groovy.io.FileType

library('sharedSpace'); // jenkins-pipeline-lib

def url = 'https://github.com/OpenSpace/OpenSpace';
//def branch = env.BRANCH_NAME;
def branch =  "feature/visual-testing-try2"

@NonCPS
def readDir() {
  def dirsl = [];
  new File("${workspace}").eachDir() {
    dirs -> println dirs.getName() 
    if (!dirs.getName().startsWith('.')) {
      dirsl.add(dirs.getName());
    }
  }
  return dirs;
}

def moduleCMakeFlags() {
  def modules = [];
  // using new File doesn't work as it is not allowed in the sandbox
  
  if (isUnix()) {
     modules = sh(returnStdout: true, script: 'ls -d modules/*').trim().split('\n');
  };
  else {
    modules = bat(returnStdout: true, script: '@dir modules /b /ad /on').trim().split('\r\n');
  }

  def flags = '';
  for (module in modules) {
      flags += "-D OPENSPACE_MODULE_${module.toUpperCase()}=ON "
  }
  return flags;
}

//
// Pipeline start
//

// parallel master: {
//   node('master') {
//     stage('master/scm') {
//       deleteDir();
//       gitHelper.checkoutGit(url, branch);
//       helper.createDirectory('build');
//     }
//     stage('master/cppcheck/create') {
//       sh 'cppcheck --enable=all --xml --xml-version=2 -i ext --suppressions-list=support/cppcheck/suppressions.txt include modules src tests 2> build/cppcheck.xml';
//     }
//     stage('master/cloc/create') {
//       sh 'cloc --by-file --exclude-dir=build,data,ext --xml --out=build/cloc.xml --force-lang-def=support/cloc/langDef --quiet .';
//     }
//   }
// },
// linux: {
//   node('linux') {
//     stage('linux/scm') {
//       deleteDir()
//       gitHelper.checkoutGit(url, branch);
//     }
//     stage('linux/build') {
//         // Not sure why the linking of OpenSpaceTest takes so long
//         compileHelper.build(compileHelper.Make(), compileHelper.Gcc(), moduleCMakeFlags(), 'OpenSpace', 'build-all');
//     }
//     stage('linux/warnings') {
//       // compileHelper.recordCompileIssues(compileHelper.Gcc());
//     }
//     stage('linux/test') {
//       // testHelper.runUnitTests('build/OpenSpaceTest');
//     }
//   } // node('linux')
// },
windows: {
  node('windows') {
    ws(branch + env.BUILD_ID) {
      stage('windows/scm') {
        deleteDir();
        gitHelper.checkoutGit(url, branch);
      }
      stage('windows/build') {
        compileHelper.build(compileHelper.VisualStudio(), compileHelper.VisualStudio(), moduleCMakeFlags(), '/p:Configuration=RelWithDebInfo', 'build-all');
      }
      stage('windows/warnings') {
        // compileHelper.recordCompileIssues(compileHelper.VisualStudio());
      }
      stage('windows/unit-tests') {
        dir('OpenSpace') {
          //testHelper.runUnitTests('bin\\RelWithDebInfo\\OpenSpaceTest')
        }
      }
      stage('windows/visual-tests') {
        dir('OpenSpace') {
          testHelper.linkFolder(env.OPENSPACE_FILES, "sync");
          testHelper.linkFolder(env.OPENSPACE_FILES, "cache_gdal");
        }
        testHelper.startTestRunner();
        testHelper.runUiTests()
        //commit new test images
        //copy test results to static dir
      }
      stage('windows/pre-package') {
        def modulesFolder = new File(workspace + "/OpenSpace/modules")
        def sha = gitHelper.getCommitSha();
        packageHelper.createOpenSpaceTree(sha);
        packageHelper.addModuleFolders(modulesFolder , sha)
      }
      stage('windows/package-archive') {
        def sha = gitHelper.getCommitSha();
        packageHelper.createOpenSpaceArchives(sha);
        //copy archives to static dir
      }
    } // node('windows')
  }
}
// osx: {
//   node('osx') {
//     stage('osx/scm') {
//       deleteDir();
//       gitHelper.checkoutGit(url, branch);
//     }
//     stage('osx/build') {
//         compileHelper.build(compileHelper.Xcode(), compileHelper.Clang(), moduleCMakeFlags(), '', 'build-all');
//     }
//     stage('osx/warnings') {
//       // compileHelper.recordCompileIssues(compileHelper.Clang());
//     }
//     stage('osx/test') {
//       // Currently, the unit tests are crashing on OS X
//       // testHelper.runUnitTests('build/Debug/OpenSpaceTest')
//     }
//   } // node('osx')
// }

// //
// // Post-build actions
// //
// node('master') {
//   stage('master/cppcheck/publish') {
//     // publishCppcheck(pattern: 'build/cppcheck.xml');
//   }
//   stage('master/cloc/publish') {
//     sloccountPublish(encoding: '', pattern: 'build/cloc.xml');
//   }
//   stage('master/notifications') {
//     slackHelper.sendChangeSetSlackMessage(currentBuild);
//   }
// }

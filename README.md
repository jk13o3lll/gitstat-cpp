# gitstat

Rely on git and need to have a copy at local.
Dependency: git, repository at local, json parser

---

How to use this tool?

First, you have to clone the repository. (For installing git, please read [the official document](https://git-scm.com/book/en/v2/Getting-Started-Installing-Git) on GIT website.

```
cd "/repositories"
git clone "aaa"

cd "/bbb"
mkdir "ccc"
cp /repository/aaa/html/css/style.css /bbb/ccc/css/
```

Template configuration file `config.json`. (there should have `.git` flie in `/repository/aaa`.

```
{
    "title": "xxxx",
    "repository": "/repositories/aaa/",
    "html": "/bbb/ccc/index.htm",
    "weights": {
        "num_commits": 0.3,
        "lines_inserted": 0.2,
        "lines_deleted": 0.15,
        "words_inserted": 0.2,
        "words_deleted": 0.15
    },
    "queries": [
        {
            "name": "Week 1",
            "duration": ["2019-09-08T00:00:00+08:00", "2019-09-14T23:59:59+08:00"]
        },
        {
            "name": "Week 2",
            "duration": ["2019-09-15T00:00:00+08:00", "2019-09-21T23:59:59+08:00"]
        },
        {
            "name": "Week 3",
            "duration": ["2019-09-22T00:00:00+08:00", "2019-09-28T23:59:59+08:00"]
        },
        {
            "name": "Week 4",
            "duration": ["2019-09-29T00:00:00+08:00", "20191-10-05T23:59:59+08:00"]
        }
    ],
    "contributors": [
        {
            "name": "Jack",
            "email": ["jack@gmail.com", "jack@yahoo.com"],
            "label": ["2019", "TA"]
        },
        {
            "name": "Bob",
            "email": ["Bob@gmail.com", "Bob2@gmail.com"],
            "label": ["2019"]
        }
    ]
}
```

Then input the configuration file to the executable file `test.exe`.

```
gitstat.exe config.json
```

To set it as routine in Linux, please use `crontab`. (ex. update every hour)

```
crontab -e
0 1 * * * gitstat.exe config.json
```
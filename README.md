# gitstat
Get statistics about commits from git service.

We use the REST API provided by bitbucket and github to obtain information about commits.

We use libcurl to send HTTP request. (We currently use curl command for convenience)

We use nlohmann/json to parson response in JSON.

// ManiVault invokes this function to set the plot data,
// when emitting qt_js_setDataInJS from the communication object
// The connection is established in qwebchannel.tools.js

var svg;

function drawChart(data) {
    // remove possible old chart 
    d3.select("div#container").select("*").remove();


    if (data != null && data.length == 0) {
        log("GeneSurfer: bar_chart.tools.js: data empty")

        // add text to the container
        d3.select("div#container")
            .append("p")
            .text("No gene available above the threshold")
            .style("font-family", "Arial")
            .style("text-align", "center")
            .attr("class", "noDataText")

        return
    }

    log("GeneSurfer: bar_chart.tools.js: draw chart")

    // set the dimensions and margins of the graph
    //var svgWidth = document.querySelector(".svg-container").offsetWidth;
    //var svgHeight = svgWidth*0.5;

    var svgWidth = 800;
    var svgHeight = 400;
       
    var margin = { top: 30, right: 30, bottom: 30, left: 30 },
        width = svgWidth - margin.left - margin.right,//800
        height = svgHeight - margin.top - margin.bottom;//400

    // append the svg object to the body of the page
    svg = d3.select("div#container")
        .append("svg")
        .attr("width", width + margin.left + margin.right)
        .attr("height", height + margin.top + margin.bottom)
        .append("g")
        .attr("transform",
            "translate(" + margin.left + "," + margin.top + ")");

    var filterUnclustered = true; // manual toggle to hide unclustered genes
    data = filterUnclustered ? data.filter(function (d) { return d.cluster !== "Unclustered"; }) : data;

    // X axis
    var x = d3.scaleBand()
        .range([0, width])
        .domain(data.map(function (d) { return d.Gene; }))
        .padding(0);

    // Y axis
    var y = d3.scaleLinear()
        //.domain([d3.min(data, function (d) { return d.Value; }), d3.max(data, function (d) { return d.Value; })])
        .domain([-1, 1])
        .range([height, 0]);

    // Add X axis at y = 0 position
    //svg.append("g")
    //    .attr("transform", "translate(0," + y(0) + ")")
    //    .call(d3.axisBottom(x))
    //    .selectAll("text")
    //    .attr("transform", "translate(-10,0)rotate(-90)")
    //    .style("text-anchor", "end");

    // Add Y axis
    svg.append("g")
        .call(d3.axisLeft(y));

    function colorToClass(color) {
        return color.replace('#', 'color_');
    }

    var tooltip = svg.append("text")
        .attr("class", "tooltip")
        .style("font-size", "20px")
        .style("font-family", "Arial")
        .attr("text-anchor", "middle")
        .attr("alignment-baseline", "middle")
        .style("opacity", 0);

    // when user hover a bar
    var mouseover = function (d) {
        var currentClass = colorToClass(d.categoryColor);
        d3.selectAll(".myRect").style("opacity", 0.2);
        d3.selectAll("." + currentClass).style("opacity", 1);
        d3.selectAll(".geneNameText").remove();// remove text from highlightBars()
        tooltip
            .style("opacity", 1)
            .text(d.Gene)
            .attr("x", x(d.Gene) + x.bandwidth() / 2)
            .attr("y", d.Value > 0 ? y(d.Value) - 20 : y(d.Value) + 20)
            .attr("fill", "black");
    }

    // When user do not hover anymore
    var mouseleave = function (d) {
        d3.selectAll(".myRect").style("opacity", 0.8)
        tooltip
            .style("opacity", 0)
    }

    // Add break symbol if filter is applied
    if (filterUnclustered) {
        var transitionPoint = data.findIndex(d => d.Value > 0);
    }

    // Bars
    svg.selectAll("mybar")
        .data(data)
        .enter()
        .append("rect")
        .attr("x", function (d, i) {
            if (filterUnclustered) {
                if (i >= transitionPoint) {
                    return x(d.Gene) + x.bandwidth();// create a gap around the break symbol if filterUnclustered is true
                }
            }
            return x(d.Gene);
        })
        .attr("y", function (d) { return d.Value > 0 ? y(d.Value) : y(0); })
        .attr("width", x.bandwidth())
        .attr("height", function (d) { return Math.abs(y(d.Value) - y(0)); })
        .attr("fill", function (d) { return d.categoryColor; })
        .attr("class", function (d) {
            return "myRect " + colorToClass(d.categoryColor);
        })
        .on("mouseover", mouseover)
        .on("mouseleave", mouseleave)
        .on("click", function (d) {
            log("Clicked on bar: " + d.Gene + " with value: " + d.Value + " and cluster: " + d.cluster);
            d3.event.stopPropagation();
            passSelectionToQt(d.Gene)//Pass selected item to ManiVault
        });

    // Draw rectangles for break symbol
    //if (filterUnclustered) {
    //    var breakSymbolHeight = 20;
    //    var offset = 3;
    //    svg.append("rect")
    //        .attr("x", x(data[transitionPoint].Gene))
    //        .attr("y", (height - breakSymbolHeight) / 2)
    //        .attr("width", x.bandwidth())
    //        .attr("height", breakSymbolHeight)
    //        .style("fill", "grey");

    //    svg.append("rect")
    //        .attr("x", x(data[transitionPoint].Gene) + offset/2)
    //        .attr("y", (height - breakSymbolHeight) / 2)
    //        .attr("width", x.bandwidth() - offset) 
    //        .attr("height", breakSymbolHeight)
    //        .style("fill", "white");
    //}
    

    // Add legend
    var clusters = Array.from(new Set(data.map(function (d) { return d.cluster; })));
    clusters.sort((a, b) => {
        if (a === "Unclustered") return 1;
        if (b === "Unclustered") return -1;
        var numA = parseInt(a.split(" ")[1]);
        var numB = parseInt(b.split(" ")[1]);
        return numA - numB;
    });

    // Map these clusters to their respective colors
    var colorMap = {};
    data.forEach(function (d) {
        colorMap[d.cluster] = d.categoryColor;
    });

    // Define a color scale using the clusters
    var colorScale = d3.scaleOrdinal()
        .domain(clusters)
        .range(clusters.map(cluster => colorMap[cluster]));

    var size = 20;

    svg.selectAll("legendDots")
        .data(clusters)
        .enter()
        .append("rect")
        .attr("x", width - 300)  // position the legend on the right side
        .attr("y", function (d, i) { return i * (size + 5) })
        .attr("width", size)
        .attr("height", size)
        .style("fill", function (d) { return colorScale(d) });

    svg.selectAll("legendLabels")
        .data(clusters)
        .enter()
        .append("text")
        .attr("x", width - 300 + size * 1.2) // position the text
        .attr("y", function (d, i) { return i * (size + 5) + (size / 2) })
        .style("font-size", "20px")
        .style("font-family", "Arial")
        .style("fill", function (d) { return colorScale(d) })
        .text(function (d) { return d })
        .attr("text-anchor", "left")
        .style("alignment-baseline", "middle");

    // TO DO: add an event listener that resizes the chart when the window is resized
}

function highlightBars(dimensions) {

    if (dimensions.length == 0) {
        log("GeneSurfer: bar_chart.tools.js: dimensions empty")
        return
    }

    log("GeneSurfer: bar_chart.tools.js: highlight bars")
    
    d3.selectAll(".myRect").style("opacity", 0.2);

    d3.selectAll(".geneNameText").remove();

    dimensions.forEach(function (dim) {
        d3.selectAll(".myRect").filter(function (d) {
            return d.Gene === dim;
        }).style("opacity", 1)
        .each(function (d) {
            var bar = d3.select(this);
            var xpos = parseFloat(bar.attr("x")) + parseFloat(bar.attr("width")) / 2
            var ypos = d.Value > 0 ? parseFloat(bar.attr("y")) - 20 : parseFloat(bar.attr("y")) + parseFloat(bar.attr("height")) + 20

            svg.append("text")
                .attr("class", "geneNameText")
                .attr("x", xpos)
                .attr("y", ypos)
                .attr("text-anchor", "middle")
                .attr("transform", function () {
                    return "rotate(-90," + xpos + "," + ypos + ")";
                })
                .text(d.Gene) 
                .attr("fill", "black") 
                .attr("font-family", "Arial")
                .attr("font-size", "10px")
                
            });
    });
}


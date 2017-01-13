<input type='radio' name='select_id' value='0' />全部逆变器
<input type='radio' name='select_id' value='1' checked='checked' />部分逆变器<br>
<table class="table table-condensed table-striped table-hover table-bordered">
    <thead>
      <tr>
        <th class="col-xs-4"><input type='checkbox' id='ids_all' /><?php echo "逆变器ID"; ?></th>
        <th class="col-xs-4"><?php echo "软件版本号"; ?></th>
        <th class="col-xs-4"><?php echo "远程更新标志"; ?></th>
      </tr>
    </thead>
    <tbody>
    <?php foreach ($ids as $key => $value): ?>
        <tr>
            <td>
                <input type='checkbox' name='ids' value='<?php echo $value[0]; ?>' />
                <?php echo $value[0]; ?>
            </td>
            <td><?php echo $value[1]; ?></td>
            <td><?php echo $value[2]; ?></td>
        </tr>
    <?php endforeach; ?>
    </tbody>
</table>

<button class='submit' id='remoteupdate'>远程更新</button><br>
<div id='divload'></div>

<script src="<?php echo base_url('resources/js/jquery-1.11.3.min.js'); ?>"></script>
<script>
$(function(){

	$('#ids_all').on('click', function(){
		if(this.checked)
			$("input[name='ids']").each(function(){this.checked=true;});
		else
			$("input[name='ids']").each(function(){this.checked=false;});
	});

	/* 设置全局Ajax默认选项 */
	$.ajaxSetup({
        type:"POST",
		dataType: "json",
		success: function(Result){
			if(Result.success) {
				alert('设置成功！');
			}
			else{
				alert('设置失败 !');
			}			
		},
		error: function(jqXHR){     
		   alert("发生错误：" + jqXHR.status);  
		}
    });
	$(document).ajaxStart(function(){
        $("#divload").html("正在设置标志位...");
    });
	$(document).ajaxStop(function(){
        $("#divload").html("标志位设置完成！");
    });

	/* 设置 */
	$('#remoteupdate').on('click', function(){
		 var ids = new Array();
        var scope = $("input:radio[name='select_id']:checked").val();
        $("input:checkbox[name='ids']:checked").each(function(){    
        	ids.push($(this).val());    
        });
        if(ids.length == 0 && scope == 1){
            alert("请选择至少一个逆变器！");
        	return false;
        }
		$.ajax({
		    url: '<?php echo base_url('index.php/hidden/set_remoteupdate');?>',
		    data: {
			    ids: ids,
			    scope: scope
			},
		});
	});
	
});
</script>